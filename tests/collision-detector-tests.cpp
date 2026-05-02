#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << ","
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace {

using namespace collision_detector;

constexpr double kEpsilon = 1e-10;

class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }

    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

}  // namespace

SCENARIO("FindGatherEvents - empty provider") {
    TestProvider provider({}, {});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - no items") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    TestProvider provider({}, {g});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - no gatherers") {
    Item item{geom::Point2D{5, 0}, 2.0};
    TestProvider provider({item}, {});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - stationary gatherer does not collide") {
    Gatherer g{geom::Point2D{5, 0}, geom::Point2D{5, 0}, 2.0};
    Item item{geom::Point2D{5, 0}, 2.0}; 
    TestProvider provider({item}, {g});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - single collision on path") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{5, 0}, 2.0};
    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, kEpsilon));
    REQUIRE_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, kEpsilon));
}

SCENARIO("FindGatherEvents - item far from path") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{5, 100}, 2.0};  
    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - item before path start") {
    Gatherer g{geom::Point2D{10, 0}, geom::Point2D{20, 0}, 2.0};
    Item item{geom::Point2D{5, 0}, 2.0};  
    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - item after path end") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{15, 0}, 2.0};  
    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - item at boundary distance") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{5, 2}, 2.0};
    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, kEpsilon));
    REQUIRE_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(4.0, kEpsilon));
}

SCENARIO("FindGatherEvents - multiple events in chronological order") {
    Gatherer g1{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Gatherer g2{geom::Point2D{0, 5}, geom::Point2D{10, 5}, 2.0};
    Item i1{geom::Point2D{2, 0}, 2.0};  
    Item i2{geom::Point2D{8, 0}, 2.0};  

    TestProvider provider({i1, i2}, {g1});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);
    REQUIRE(events[0].time <= events[1].time + kEpsilon);
    REQUIRE(events[0].item_id == 0);  
    REQUIRE(events[1].item_id == 1);
}

SCENARIO("FindGatherEvents - same item collected by two gatherers") {
    Item item{geom::Point2D{5, 0}, 2.0};
    Gatherer g1{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Gatherer g2{geom::Point2D{10, 0}, geom::Point2D{0, 0}, 2.0};

    TestProvider provider({item}, {g1, g2});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);
    REQUIRE((events[0].item_id == 0 && events[1].item_id == 0));
    REQUIRE((events[0].gatherer_id == 0 || events[0].gatherer_id == 1));
    REQUIRE((events[1].gatherer_id == 0 || events[1].gatherer_id == 1));
    REQUIRE(events[0].gatherer_id != events[1].gatherer_id);
}

SCENARIO("FindGatherEvents - events have correct indices") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item i0{geom::Point2D{3, 0}, 2.0};
    Item i1{geom::Point2D{7, 0}, 2.0};

    TestProvider provider({i0, i1}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);
    std::vector<size_t> item_ids = {events[0].item_id, events[1].item_id};
    REQUIRE(std::find(item_ids.begin(), item_ids.end(), 0) != item_ids.end());
    REQUIRE(std::find(item_ids.begin(), item_ids.end(), 1) != item_ids.end());
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[1].gatherer_id == 0);
}

SCENARIO("FindGatherEvents - time in valid range") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{5, 1}, 2.0};

    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].time >= 0.0 - kEpsilon);
    REQUIRE(events[0].time <= 1.0 + kEpsilon);
}

SCENARIO("FindGatherEvents - diagonal movement") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 10}, 2.0};
    Item item{geom::Point2D{5, 5}, 2.0};  

    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, kEpsilon));
    REQUIRE_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, kEpsilon));
}

SCENARIO("FindGatherEvents - item just outside collision radius") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{10, 0}, 2.0};
    Item item{geom::Point2D{5, 5}, 2.0};

    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.empty());
}

SCENARIO("FindGatherEvents - small nonzero movement") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D(1e-10, 0), 2.0};
    Item item{geom::Point2D(5e-11, 0), 2.0};

    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
}

SCENARIO("FindGatherEvents - exact expected values for known setup") {
    Gatherer g{geom::Point2D{0, 0}, geom::Point2D{4, 0}, 1.0};
    Item item{geom::Point2D{2, 1}, 1.0};

    TestProvider provider({item}, {g});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, kEpsilon));
    REQUIRE_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(1.0, kEpsilon));
}
