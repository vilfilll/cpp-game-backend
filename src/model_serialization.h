#pragma once

#include <boost/serialization/vector.hpp>

#include "geom.h"
#include "model.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, Position& p, [[maybe_unused]] const unsigned version) {
    ar& p.x;
    ar& p.y;
}

template <typename Archive>
void serialize(Archive& ar, Speed& s, [[maybe_unused]] const unsigned version) {
    ar& s.x;
    ar& s.y;
}

template <typename Archive>
void serialize(Archive& ar, Direction& d, [[maybe_unused]] const unsigned version) {
    int v = static_cast<int>(d);
    ar& v;
    d = static_cast<Direction>(v);
}

template <typename Archive>
void serialize(Archive& ar, BagItem& b, [[maybe_unused]] const unsigned version) {
    ar& b.id;
    ar& b.type;
}

}  // namespace model

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(*dog.GetId())
        , name_(dog.GetName())
        , position_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_(dog.GetBag()) {
    }

    [[nodiscard]] std::unique_ptr<model::Dog> RestorePtr() const {
        auto dog = std::make_unique<model::Dog>(model::Dog::Id{id_}, name_);
        dog->SetPosition(position_.x, position_.y);
        dog->SetSpeed(speed_.x, speed_.y);
        dog->SetDirection(direction_);
        if (score_ != 0) {
            dog->AddToScore(score_);
        }
        for (const auto& item : bag_) {
            dog->AddToBag(item.id, item.type);
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& position_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& bag_;
    }

private:
    std::size_t id_ = 0;
    std::string name_;
    model::Position position_{};
    model::Speed speed_{};
    model::Direction direction_ = model::Direction::NORTH;
    int score_ = 0;
    std::vector<model::BagItem> bag_;
};

struct LootRepr {
    LootRepr() = default;

    explicit LootRepr(const model::Loot& loot)
        : id_(*loot.id)
        , type_(loot.type)
        , position_(loot.position) {
    }

    [[nodiscard]] model::Loot ToLoot() const {
        return model::Loot{model::Loot::Id{id_}, type_, position_};
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned file_version) {
        ar& id_;
        ar& type_;
        ar& position_;
    }

private:
    std::size_t id_ = 0;
    int type_ = 0;
    model::Position position_{};
};

}  // namespace serialization
