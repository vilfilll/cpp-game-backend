#include "application.h"
#include "model_serialization.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>

#include <istream>
#include <ostream>
#include <stdexcept>
#include <cmath>

namespace app {

namespace {

constexpr double kSpeedEpsilon = 1e-9;

inline bool IsDogStopped(const model::Dog& dog) noexcept {
    return std::fabs(dog.GetSpeed().x) < kSpeedEpsilon && std::fabs(dog.GetSpeed().y) < kSpeedEpsilon;
}

}  // namespace

std::chrono::milliseconds Application::MakeRetirementDurationMs(double dog_retirement_time_sec) noexcept {
    const double d =
        (dog_retirement_time_sec > 0.0 && std::isfinite(dog_retirement_time_sec)) ? dog_retirement_time_sec : 60.0;
    // Same as Python int(seconds * 1000) used by the official tests for tick deltas.
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(d * 1000.0));
}

namespace {

struct SavedSession {
    std::string map_id;
    std::vector<serialization::DogRepr> dogs;
    std::size_t next_dog_id = 0;
    std::vector<serialization::LootRepr> loot;
    std::size_t next_loot_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned file_version) {
        ar& map_id;
        ar& dogs;
        ar& next_dog_id;
        ar& loot;
        ar& next_loot_id;
    }
};

struct SavedPlayer {
    Player::Id player_id = 0;
    std::string map_id;
    std::size_t dog_id = 0;
    std::string token;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned file_version) {
        ar& player_id;
        ar& map_id;
        ar& dog_id;
        ar& token;
    }
};

struct SavedRoot {
    unsigned version = 1;
    std::vector<SavedSession> sessions;
    Player::Id next_player_id = 0;
    std::vector<SavedPlayer> players;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned file_version) {
        ar& version;
        ar& sessions;
        ar& next_player_id;
        ar& players;
    }
};

model::Dog* FindDogInSession(model::GameSession& session, std::size_t dog_id) {
    for (const auto& d : session.GetDogs()) {
        if (d && *d->GetId() == dog_id) {
            return d.get();
        }
    }
    return nullptr;
}

}  // namespace

model::GameSession& Application::GetOrCreateSession(const model::Map::Id& map_id) {
    return game_.GetOrCreateSession(map_id);
}

JoinResult Application::JoinGame(const model::Map::Id& map_id, std::string user_name) {
    model::GameSession& session = GetOrCreateSession(map_id);
    model::Dog& dog = session.AddDog(user_name);
    const auto& roads = session.GetMap().GetRoads();
    if (!roads.empty()) {
        if (game_.IsRandomSpawnMode()) {
            std::uniform_int_distribution<std::size_t> road_index_dist(0, roads.size() - 1);
            const auto& road = roads[road_index_dist(random_engine_)];
            const auto start = road.GetStart();
            const auto end = road.GetEnd();

            double x = 0.0;
            double y = 0.0;

            if (road.IsHorizontal()) {
                const auto x_min = static_cast<double>(std::min(start.x, end.x));
                const auto x_max = static_cast<double>(std::max(start.x, end.x));
                std::uniform_real_distribution<double> dist(x_min, x_max);
                x = dist(random_engine_);
                y = static_cast<double>(start.y);
            } else {
                const auto y_min = static_cast<double>(std::min(start.y, end.y));
                const auto y_max = static_cast<double>(std::max(start.y, end.y));
                std::uniform_real_distribution<double> dist(y_min, y_max);
                x = static_cast<double>(start.x);
                y = dist(random_engine_);
            }

            dog.SetPosition(x, y);
        } else {
            const auto& road = roads.front();
            const auto start = road.GetStart();
            dog.SetPosition(static_cast<double>(start.x), static_cast<double>(start.y));
        }
    }
    dog.SetSpeed(0.0, 0.0);
    dog.SetDirection(model::Direction::NORTH);
    Player& player = players_.Add(dog, session);
    auto token = tokens_.AddPlayer(player);
    retirement_[player.GetId()] = RetirementState{game_elapsed_, game_elapsed_};
    return JoinResult{player.GetId(), *token};
}

std::vector<PlayerInfo> Application::GetPlayersByToken(std::string_view token) const {
    Player* me = tokens_.FindPlayerByToken(token);
    if (!me) {
        throw std::out_of_range("token not found");
    }
    const auto players = players_.GetPlayersInSession(me->GetSession());
    std::vector<PlayerInfo> result;
    result.reserve(players.size());
    for (const Player* p : players) {
        const auto& dog = p->GetDog();
        std::vector<model::BagItem> bag(dog.GetBag().begin(), dog.GetBag().end());
        result.push_back(PlayerInfo{
            p->GetId(),
            dog.GetName(),
            dog.GetPosition(),
            dog.GetSpeed(),
            dog.GetDirection(),
            std::move(bag),
            dog.GetScore()});
    }
    return result;
}

void Application::SetPlayerAction(std::string_view token, std::string_view move) {
    Player* player = tokens_.FindPlayerByToken(token);
    if (!player) {
        throw std::out_of_range("token not found");
    }
    const double s = player->GetSession().GetMap().GetDogSpeed();
    auto& dog = player->GetDog();

    if (move == "L") {
        dog.SetSpeed(-s, 0);
        dog.SetDirection(model::Direction::WEST);
    } else if (move == "R") {
        dog.SetSpeed(s, 0);
        dog.SetDirection(model::Direction::EAST);
    } else if (move == "U") {
        dog.SetSpeed(0, -s);
        dog.SetDirection(model::Direction::NORTH);
    } else if (move == "D") {
        dog.SetSpeed(0, s);
        dog.SetDirection(model::Direction::SOUTH);
    } else if (move == "") {
        dog.SetSpeed(0, 0);
    } else {
        throw std::invalid_argument("invalid move");
    }

    if (auto it = retirement_.find(player->GetId()); it != retirement_.end()) {
        if (move != "") {
            it->second.idle_since_ms = std::nullopt;
        } else {
            it->second.idle_since_ms = game_elapsed_;
        }
    }
}

void Application::Tick(std::chrono::milliseconds delta) {
    game_.Tick(delta);
    if (delta.count() <= 0) {
        if (tick_listener_) {
            tick_listener_->OnTick(delta);
        }
        return;
    }
    game_elapsed_ += delta;
    ProcessRetirements();
    if (tick_listener_) {
        tick_listener_->OnTick(delta);
    }
}

void Application::OnShutdownSave() {
    if (tick_listener_) {
        tick_listener_->OnShutdownSave();
    }
}

void Application::SaveState(std::ostream& os) const {
    SavedRoot root;
    root.version = 1;
    root.next_player_id = players_.GetNextPlayerId();

    for (const auto& [map_id, session_ptr] : game_.GetSessions()) {
        if (!session_ptr) {
            continue;
        }
        SavedSession ss;
        ss.map_id = *map_id;
        ss.next_dog_id = session_ptr->GetNextDogIdCounter();
        ss.next_loot_id = session_ptr->GetNextLootIdCounter();
        for (const auto& dog_ptr : session_ptr->GetDogs()) {
            if (dog_ptr) {
                ss.dogs.emplace_back(*dog_ptr);
            }
        }
        for (const auto& l : session_ptr->GetLoot()) {
            ss.loot.emplace_back(l);
        }
        root.sessions.push_back(std::move(ss));
    }

    for (const Player* player_ptr : players_.GetAllPlayers()) {
        SavedPlayer sp;
        sp.player_id = player_ptr->GetId();
        sp.map_id = *player_ptr->GetSession().GetMap().GetId();
        sp.dog_id = *player_ptr->GetDog().GetId();
        if (auto tok = tokens_.FindTokenForPlayer(player_ptr)) {
            sp.token = std::move(*tok);
        } else {
            throw std::runtime_error("internal error: player has no token");
        }
        root.players.push_back(std::move(sp));
    }

    boost::archive::text_oarchive oa{os};
    oa << root;
}

void Application::LoadState(std::istream& is) {
    SavedRoot root;
    boost::archive::text_iarchive ia{is};
    ia >> root;

    if (root.version != 1u) {
        throw std::runtime_error("unsupported state file version");
    }

    players_.Clear();
    tokens_.Clear();

    for (const auto& ss : root.sessions) {
        model::GameSession& session = GetOrCreateSession(model::Map::Id{ss.map_id});
        std::vector<std::unique_ptr<model::Dog>> dogs;
        dogs.reserve(ss.dogs.size());
        for (const auto& repr : ss.dogs) {
            dogs.push_back(repr.RestorePtr());
        }
        std::vector<model::Loot> loot_vec;
        loot_vec.reserve(ss.loot.size());
        for (const auto& lr : ss.loot) {
            loot_vec.push_back(lr.ToLoot());
        }
        session.ReplaceDynamicStateForRestore(std::move(dogs), ss.next_dog_id, std::move(loot_vec), ss.next_loot_id);
    }

    for (const auto& sp : root.players) {
        if (sp.token.empty()) {
            throw std::runtime_error("invalid state file: empty token");
        }
        model::GameSession& session = GetOrCreateSession(model::Map::Id{sp.map_id});
        model::Dog* dog = FindDogInSession(session, sp.dog_id);
        if (!dog) {
            throw std::runtime_error("dog not found when restoring state");
        }
        Player& player = players_.RestorePlayer(sp.player_id, *dog, session);
        tokens_.RestoreMapping(sp.token, player);
    }

    players_.SetNextPlayerId(root.next_player_id);
    InitRetirementForAllPlayers();
}

std::vector<model::Loot> Application::GetLostObjects(std::string_view token) const {
    const Player* player = tokens_.FindPlayerByToken(token);

    if (!player) {
        return {};
    }

    const auto& session = player->GetSession();
    const auto& loot = session.GetLoot();

    return {loot.begin(), loot.end()};
}

void Application::ProcessRetirements() {
    const auto now_ms = game_elapsed_;
    const auto active = players_.GetAllMutablePlayers();
    std::vector<Player::Id> retire_ids;
    retire_ids.reserve(active.size());

    for (Player* p : active) {
        auto it = retirement_.find(p->GetId());
        if (it == retirement_.end()) {
            continue;
        }
        auto& st = it->second;
        const auto& dog = p->GetDog();
        if (!IsDogStopped(dog)) {
            st.idle_since_ms = std::nullopt;
            continue;
        }
        if (!st.idle_since_ms) {
            st.idle_since_ms = now_ms;
        }
        if (now_ms - *st.idle_since_ms >= retirement_duration_ms_) {
            retire_ids.push_back(p->GetId());
        }
    }

    for (Player::Id id : retire_ids) {
        if (Player* p = players_.FindById(id)) {
            RetirePlayer(*p);
        }
    }
}

void Application::RetirePlayer(Player& player) {
    const Player::Id pid = player.GetId();
    auto it = retirement_.find(pid);
    const std::chrono::milliseconds join_ms =
        (it != retirement_.end()) ? it->second.join_time_ms : std::chrono::milliseconds{0};
    const std::chrono::milliseconds now_ms = game_elapsed_;
    const double play_time = std::chrono::duration<double>(now_ms - join_ms).count();

    auto& dog = player.GetDog();
    const std::string name = dog.GetName();
    const int score = dog.GetScore();
    auto& session = player.GetSession();
    const auto dog_id = dog.GetId();

    if (retirement_db_) {
        retirement_db_->InsertRetired(name, score, play_time);
    }

    tokens_.RemovePlayerMapping(&player);
    session.RemoveDog(dog_id);
    players_.RemovePlayer(&player);
    retirement_.erase(pid);
}

void Application::InitRetirementForAllPlayers() {
    retirement_.clear();
    const auto now_ms = game_elapsed_;
    for (Player* p : players_.GetAllMutablePlayers()) {
        const auto& dog = p->GetDog();
        const bool stopped = IsDogStopped(dog);
        retirement_[p->GetId()] = RetirementState{
            now_ms,
            stopped ? std::optional<std::chrono::milliseconds>(now_ms) : std::nullopt};
    }
}

std::vector<db::RetiredPlayerRow> Application::ListRetiredRecords(int start, int max_items) const {
    if (!retirement_db_) {
        return {};
    }
    return retirement_db_->FetchRecords(start, max_items);
}

}
