#include "model.h"
#include "collision_detector.h"
#include "geom.h"

#include <algorithm>
#include <iomanip>
#include <unordered_set>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {
constexpr double kRoadHalfWidth = 0.4;
constexpr double kEpsilon = 1e-9;

struct Interval {
    double lo = 0.0;
    double hi = 0.0;
};

constexpr Interval EMPTY_INTERVAL{1.0, 0.0};

std::vector<Interval> MergeIntervals(std::vector<Interval> ivs) {
    if (ivs.empty()) {
        return {};
    }
    std::sort(ivs.begin(), ivs.end(), [](const Interval& a, const Interval& b) { return a.lo < b.lo; });
    std::vector<Interval> out;
    out.reserve(ivs.size());
    out.push_back(ivs.front());
    for (size_t i = 1; i < ivs.size(); ++i) {
        auto& last = out.back();
        if (ivs[i].lo <= last.hi) {
            last.hi = std::max(last.hi, ivs[i].hi);
        } else {
            out.push_back(ivs[i]);
        }
    }
    return out;
}

std::optional<Interval> FindContaining(const std::vector<Interval>& merged, double value) {
    for (const auto& iv : merged) {
        if (iv.lo <= value && value <= iv.hi) {
            return iv;
        }
    }
    return std::nullopt;
}

Interval GetRoadXIntervalAtY(const Road& road, double y) {
    const auto s = road.GetStart();
    const auto e = road.GetEnd();
    if (road.IsHorizontal()) {
        const double y0 = static_cast<double>(s.y);
        if (y < y0 - kRoadHalfWidth || y > y0 + kRoadHalfWidth) {
            return {1.0, 0.0};  
        }
        const double x_min = static_cast<double>(std::min(s.x, e.x)) - kRoadHalfWidth;
        const double x_max = static_cast<double>(std::max(s.x, e.x)) + kRoadHalfWidth;
        return {x_min, x_max};
    }

    const double x0 = static_cast<double>(s.x);
    const double y_min = static_cast<double>(std::min(s.y, e.y));
    const double y_max = static_cast<double>(std::max(s.y, e.y));
    if (y < y_min - kRoadHalfWidth || y > y_max + kRoadHalfWidth) {
        return EMPTY_INTERVAL;
    }
    return {x0 - kRoadHalfWidth, x0 + kRoadHalfWidth};
}

Interval GetRoadYIntervalAtX(const Road& road, double x) {
    const auto s = road.GetStart();
    const auto e = road.GetEnd();
    if (road.IsVertical()) {
        const double x0 = static_cast<double>(s.x);
        if (x < x0 - kRoadHalfWidth || x > x0 + kRoadHalfWidth) {
            return {1.0, 0.0};
        }
        const double y_min = static_cast<double>(std::min(s.y, e.y)) - kRoadHalfWidth;
        const double y_max = static_cast<double>(std::max(s.y, e.y)) + kRoadHalfWidth;
        return {y_min, y_max};
    }
  
    const double y0 = static_cast<double>(s.y);
    const double x_min = static_cast<double>(std::min(s.x, e.x));
    const double x_max = static_cast<double>(std::max(s.x, e.x));
    if (x < x_min - kRoadHalfWidth || x > x_max + kRoadHalfWidth) {
        return EMPTY_INTERVAL;
    }
    return {y0 - kRoadHalfWidth, y0 + kRoadHalfWidth};
}

std::optional<Interval> GetAllowedXRangeAtY(const Map& map, double x, double y) {
    std::vector<Interval> ivs;
    ivs.reserve(map.GetRoads().size());
    for (const auto& road : map.GetRoads()) {
        const auto iv = GetRoadXIntervalAtY(road, y);
        if (iv.lo <= iv.hi + kEpsilon) {
            ivs.push_back(iv);
        }
    }
    const auto merged = MergeIntervals(std::move(ivs));
    return FindContaining(merged, x);
}

std::optional<Interval> GetAllowedYRangeAtX(const Map& map, double x, double y) {
    std::vector<Interval> ivs;
    ivs.reserve(map.GetRoads().size());
    for (const auto& road : map.GetRoads()) {
        const auto iv = GetRoadYIntervalAtX(road, x);
        if (iv.lo <= iv.hi + kEpsilon) {
            ivs.push_back(iv);
        }
    }
    const auto merged = MergeIntervals(std::move(ivs));
    return FindContaining(merged, y);
}

void MoveDog(const Map& map, Dog& dog, std::chrono::milliseconds dt) {
    const auto pos = dog.GetPosition();
    const auto spd = dog.GetSpeed();
    if (spd.x == 0.0 && spd.y == 0.0) {
        return;
    }

    const double t = std::chrono::duration<double>(dt).count();
    if (t <= 0.0) {
        return;
    }

    if (spd.x != 0.0) {
        auto allowed = GetAllowedXRangeAtY(map, pos.x, pos.y);
        if (!allowed) {
            dog.SetSpeed(0.0, 0.0);
            return;
        }
        const double target_x = pos.x + spd.x * t;
        if (target_x < allowed->lo) {
            dog.SetPosition(allowed->lo, pos.y);
            dog.SetSpeed(0.0, 0.0);
        } else if (target_x > allowed->hi) {
            dog.SetPosition(allowed->hi, pos.y);
            dog.SetSpeed(0.0, 0.0);
        } else {
            dog.SetPosition(target_x, pos.y);
        }
        return;
    }

    auto allowed = GetAllowedYRangeAtX(map, pos.x, pos.y);
    if (!allowed) {
        dog.SetSpeed(0.0, 0.0);
        return;
    }
    const double target_y = pos.y + spd.y * t;
    if (target_y < allowed->lo) {
        dog.SetPosition(pos.x, allowed->lo);
        dog.SetSpeed(0.0, 0.0);
    } else if (target_y > allowed->hi) {
        dog.SetPosition(pos.x, allowed->hi);
        dog.SetSpeed(0.0, 0.0);
    } else {
        dog.SetPosition(pos.x, target_y);
    }
}

}  // namespace

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
       
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

Dog& GameSession::AddDog(std::string name) {
    auto dog = std::make_unique<Dog>(Dog::Id{next_dog_id_++}, std::move(name));
    Dog& ref = *dog;
    dogs_.push_back(std::move(dog));
    return ref;
}

void GameSession::RemoveLoot(Loot::Id id) {
    loot_.erase(
        std::remove_if(loot_.begin(), loot_.end(),
                       [&id](const Loot& l) { return l.id == id; }),
        loot_.end());
}

void GameSession::ReplaceDynamicStateForRestore(std::vector<std::unique_ptr<Dog>> dogs,
                                                std::size_t next_dog_id,
                                                std::vector<Loot> loot,
                                                std::size_t next_loot_id) {
    dogs_ = std::move(dogs);
    next_dog_id_ = next_dog_id;
    loot_ = std::move(loot);
    next_loot_id_ = next_loot_id;
}

void GameSession::RemoveDog(const Dog::Id& id) {
    dogs_.erase(
        std::remove_if(dogs_.begin(), dogs_.end(),
                       [&id](const std::unique_ptr<Dog>& d) {
                           return d && d->GetId() == id;
                       }),
        dogs_.end());
}

void GameSession::GenerateLoot(std::chrono::milliseconds dt, std::mt19937& rng,
                size_t loot_types_count) {

    unsigned new_loot = generator_.Generate(
        dt,
        loot_.size(),
        dogs_.size()
    );

    if (new_loot == 0) {
        return;
    }

    const auto& roads = map_->GetRoads();

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    std::uniform_int_distribution<int> type_dist(0, loot_types_count - 1);

    for (unsigned i = 0; i < new_loot; ++i) {

        const Road& road = roads[road_dist(rng)];

        Position pos;

        if (road.IsHorizontal()) {

            double x0 = std::min(road.GetStart().x, road.GetEnd().x);
            double x1 = std::max(road.GetStart().x, road.GetEnd().x);

            std::uniform_real_distribution<double> x_dist(x0, x1);

            pos.x = x_dist(rng);
            pos.y = road.GetStart().y;

        } else {

            double y0 = std::min(road.GetStart().y, road.GetEnd().y);
            double y1 = std::max(road.GetStart().y, road.GetEnd().y);

            std::uniform_real_distribution<double> y_dist(y0, y1);

            pos.y = y_dist(rng);
            pos.x = road.GetStart().x;
        }

        int type = type_dist(rng);

        loot_.emplace_back(Loot::Id{next_loot_id_++}, type, pos);
    }
}

GameSession& Game::GetOrCreateSession(const Map::Id& map_id) {
    if (auto it = sessions_.find(map_id); it != sessions_.end()) {
        return *it->second;
    }

    const Map* map = FindMap(map_id);
    if (!map) {
        throw std::out_of_range("map not found");
    }

    loot_gen::LootGenerator generator(
        loot_gen_period_,
        loot_gen_probability_,
        [this]() {
            return std::generate_canonical<double, 10>(random_engine_);
        }
    );

    auto session = std::make_unique<GameSession>(*map, std::move(generator));
    GameSession& ref = *session;
    sessions_.emplace(map_id, std::move(session));
    
    return ref;
}

namespace {

constexpr double kPlayerHalfWidth = 0.3;   
constexpr double kLootHalfWidth = 0.0;
constexpr double kOfficeHalfWidth = 0.25;  

using namespace collision_detector;

class LootGatherProvider : public ItemGathererProvider {
public:
    LootGatherProvider(const std::vector<Loot>& loot,
                       const std::vector<std::pair<Position, Position>>& dog_moves)
        : loot_(loot)
        , dog_moves_(dog_moves) {
        items_.reserve(loot_.size());
        for (const auto& l : loot_) {
            items_.push_back(Item{geom::Point2D{l.position.x, l.position.y}, kLootHalfWidth});
        }
        gatherers_.reserve(dog_moves_.size());
        for (const auto& m : dog_moves_) {
            gatherers_.push_back(Gatherer{
                geom::Point2D{m.first.x, m.first.y},
                geom::Point2D{m.second.x, m.second.y},
                kPlayerHalfWidth});
        }
    }

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    const std::vector<Loot>& loot_;
    const std::vector<std::pair<Position, Position>>& dog_moves_;
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

class OfficeGatherProvider : public ItemGathererProvider {
public:
    OfficeGatherProvider(const std::vector<Office>& offices,
                        const std::vector<std::pair<Position, Position>>& dog_moves)
        : dog_moves_(dog_moves) {
        items_.reserve(offices.size());
        for (const auto& o : offices) {
            auto p = o.GetPosition();
            items_.push_back(Item{geom::Point2D{static_cast<double>(p.x), static_cast<double>(p.y)},
                                  kOfficeHalfWidth});
        }
        gatherers_.reserve(dog_moves_.size());
        for (const auto& m : dog_moves_) {
            gatherers_.push_back(Gatherer{
                geom::Point2D{m.first.x, m.first.y},
                geom::Point2D{m.second.x, m.second.y},
                kPlayerHalfWidth});
        }
    }

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    const std::vector<std::pair<Position, Position>>& dog_moves_;
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

struct TickEvent {
    enum class Type { LOOT, OFFICE };
    Type type;
    size_t item_id;
    size_t gatherer_id;
    double time;
};

void ProcessSessionGathering(GameSession& session, const Map& map,
                          const std::vector<std::pair<Position, Position>>& dog_moves,
                          size_t bag_capacity) {
    const auto loot_snapshot = session.GetLoot();
    if (loot_snapshot.empty() && map.GetOffices().empty()) {
        return;
    }

    std::vector<TickEvent> events;

    if (!loot_snapshot.empty()) {
        LootGatherProvider loot_provider(loot_snapshot, dog_moves);
        auto loot_evts = FindGatherEvents(loot_provider);
        for (const auto& e : loot_evts) {
            events.push_back(TickEvent{TickEvent::Type::LOOT, e.item_id, e.gatherer_id, e.time});
        }
    }

    if (!map.GetOffices().empty()) {
        OfficeGatherProvider office_provider(map.GetOffices(), dog_moves);
        auto office_evts = FindGatherEvents(office_provider);
        for (const auto& e : office_evts) {
            events.push_back(TickEvent{TickEvent::Type::OFFICE, e.item_id, e.gatherer_id, e.time});
        }
    }

    std::sort(events.begin(), events.end(),
              [](const TickEvent& a, const TickEvent& b) { return a.time < b.time; });

    std::unordered_set<std::size_t> collected_loot_ids;
    const auto& dogs = session.GetDogs();

    for (const auto& evt : events) {
        if (evt.gatherer_id >= dogs.size() || !dogs[evt.gatherer_id]) {
            continue;
        }
        Dog& dog = *dogs[evt.gatherer_id];

        if (evt.type == TickEvent::Type::OFFICE) {
            int bag_value = 0;
            for (const auto& item : dog.GetBag()) {
                bag_value += map.GetLootTypeValue(static_cast<size_t>(item.type));
            }
            dog.AddToScore(bag_value);
            dog.ClearBag();
        } else {
            if (evt.item_id >= loot_snapshot.size()) continue;
            const auto& loot = loot_snapshot[evt.item_id];
            std::size_t loot_id = *loot.id;
            if (collected_loot_ids.count(loot_id)) continue;
            if (dog.BagSize() >= bag_capacity) continue;

            dog.AddToBag(loot_id, loot.type);
            collected_loot_ids.insert(loot_id);
            session.RemoveLoot(loot.id);
        }
    }
}

}  // namespace

void Game::Tick(std::chrono::milliseconds time_delta) {
    if (time_delta.count() <= 0) {
        return;
    }
    for (auto& [map_id, session_ptr] : sessions_) {
        if (!session_ptr) {
            continue;
        }
        const Map& map = session_ptr->GetMap();
        const auto& dogs = session_ptr->GetDogs();
        const size_t bag_cap = map.GetBagCapacity().value_or(GetDefaultBagCapacity());

        std::vector<std::pair<Position, Position>> dog_moves(dogs.size());
        for (size_t i = 0; i < dogs.size(); ++i) {
            if (dogs[i]) {
                const auto& pos = dogs[i]->GetPosition();
                dog_moves[i] = {{pos.x, pos.y}, {pos.x, pos.y}};
            }
        }

        for (size_t i = 0; i < dogs.size(); ++i) {
            if (dogs[i]) {
                MoveDog(map, *dogs[i], time_delta);
                const auto& end = dogs[i]->GetPosition();
                dog_moves[i].second = Position{end.x, end.y};
            }
        }

        ProcessSessionGathering(*session_ptr, map, dog_moves, bag_cap);

        session_ptr->GenerateLoot(
            time_delta,
            random_engine_,
            session_ptr->GetMap().GetLootTypesCount()
        );
    }
}

}  // namespace model
