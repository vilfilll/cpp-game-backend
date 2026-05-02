#pragma once
#include <compare>
#include <chrono>
#include <optional>
#include <random>
#include <string_view>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include "tagged.h"
#include "loot_generator.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Position {
    double x;
    double y;
};

struct Speed {
    double x;
    double y;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, double dog_speed = 1.0) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , dog_speed_(dog_speed) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void SetLootTypesCount(size_t count) noexcept {
        loot_types_count_ = count;
    }

    size_t GetLootTypesCount() const noexcept {
        return loot_types_count_;
    }

    void AddLootTypeValue(int value) {
        loot_type_values_.push_back(value);
    }

    int GetLootTypeValue(size_t index) const noexcept {
        if (index < loot_type_values_.size()) {
            return loot_type_values_[index];
        }
        return 0;
    }

    void SetBagCapacity(size_t capacity) noexcept {
        bag_capacity_ = capacity;
    }

    std::optional<size_t> GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_;
    std::optional<size_t> bag_capacity_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    size_t loot_types_count_ = 0;
    std::vector<int> loot_type_values_;
};

struct BagItem {
    std::size_t id;
    int type;

    auto operator<=>(const BagItem&) const = default;
};

class Dog {
public:
    using Id = util::Tagged<std::size_t, Dog>;

    Dog(Id id, std::string name)
        : id_(id)
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Position& GetPosition() const noexcept {
        return position_;
    }

    const Speed& GetSpeed() const noexcept {
        return speed_;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetPosition(double x, double y) noexcept {
        position_.x = x;
        position_.y = y;
    }

    void SetSpeed(double x, double y) noexcept {
        speed_.x = x;
        speed_.y = y;
    }

    void SetDirection(Direction dir) noexcept {
        direction_ = dir;
    }

    const std::vector<BagItem>& GetBag() const noexcept {
        return bag_;
    }

    void AddToBag(std::size_t id, int type) {
        bag_.emplace_back(BagItem{id, type});
    }

    void ClearBag() noexcept {
        bag_.clear();
    }

    size_t BagSize() const noexcept {
        return bag_.size();
    }

    int GetScore() const noexcept {
        return score_;
    }

    void AddToScore(int value) noexcept {
        score_ += value;
    }

private:
    Id id_;
    std::string name_;
    Position position_{0.0, 0.0};
    Speed speed_{0.0, 0.0};
    Direction direction_{Direction::NORTH};
    std::vector<BagItem> bag_;
    int score_ = 0;
};

struct Loot {
    using Id = util::Tagged<std::size_t, Loot>;

    Id id;
    int type;
    Position position;
};

class GameSession {
public:
    explicit GameSession(const Map& map, loot_gen::LootGenerator generator) noexcept
        : map_(&map) 
        , generator_(std::move(generator)) {
    }

    const Map& GetMap() const noexcept {
        return *map_;
    }

    Dog& AddDog(std::string name);

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

    const std::vector<Loot>& GetLoot() const noexcept {
        return loot_;
    }

    void AddLoot(int type, Position pos) {
        loot_.push_back({Loot::Id{next_loot_id_++}, type, pos});
    }

    void RemoveLoot(Loot::Id id);

    void GenerateLoot(std::chrono::milliseconds dt, std::mt19937& rng,
                    size_t loot_types_count);

    void ReplaceDynamicStateForRestore(std::vector<std::unique_ptr<Dog>> dogs,
                                       std::size_t next_dog_id,
                                       std::vector<Loot> loot,
                                       std::size_t next_loot_id);

    void RemoveDog(const Dog::Id& id);

    std::size_t GetNextDogIdCounter() const noexcept {
        return next_dog_id_;
    }

    std::size_t GetNextLootIdCounter() const noexcept {
        return next_loot_id_;
    }

private:
    const Map* map_;
    std::vector<std::unique_ptr<Dog>> dogs_;
    std::size_t next_dog_id_ = 0;

    std::vector<Loot> loot_;
    std::size_t next_loot_id_ = 0;

    loot_gen::LootGenerator generator_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    void Tick(std::chrono::milliseconds time_delta);

    void SetRandomSpawnMode(bool value) noexcept {
        randomize_spawn_points_ = value;
    }

    bool IsRandomSpawnMode() const noexcept {
        return randomize_spawn_points_;
    }

    void SetLootGeneratorConfig(std::chrono::milliseconds period, double probability) {
        loot_gen_period_ = period;
        loot_gen_probability_ = probability;
    }

    std::chrono::milliseconds GetLootGenPeriod() const noexcept {
        return loot_gen_period_;
    }

    double GetLootGenProbability() const noexcept {
        return loot_gen_probability_;
    }

    void SetDefaultBagCapacity(size_t capacity) noexcept {
        default_bag_capacity_ = capacity;
    }

    size_t GetDefaultBagCapacity() const noexcept {
        return default_bag_capacity_;
    }

    GameSession& GetOrCreateSession(const Map::Id& map_id);

    const std::unordered_map<Map::Id, std::unique_ptr<GameSession>, util::TaggedHasher<Map::Id>>& GetSessions() const noexcept {
        return sessions_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    std::unordered_map<Map::Id, std::unique_ptr<GameSession>, MapIdHasher> sessions_;
    bool randomize_spawn_points_ = false;
    std::mt19937 random_engine_{std::random_device{}()};

    std::chrono::milliseconds loot_gen_period_{1000};
    double loot_gen_probability_ = 0.0;
    size_t default_bag_capacity_ = 3;
};

}  // namespace model
