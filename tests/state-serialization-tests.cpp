#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{Dog::Id{42}, "Pluto"s};
            dog.SetPosition(42.2, 12.5);
            dog.AddToScore(42);
            dog.AddToBag(10, 2);
            dog.SetDirection(Direction::EAST);
            dog.SetSpeed(2.3, -1.2);
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.RestorePtr();

                CHECK(dog.GetId() == restored->GetId());
                CHECK(dog.GetName() == restored->GetName());
                CHECK(dog.GetPosition().x == restored->GetPosition().x);
                CHECK(dog.GetPosition().y == restored->GetPosition().y);
                CHECK(dog.GetSpeed().x == restored->GetSpeed().x);
                CHECK(dog.GetSpeed().y == restored->GetSpeed().y);
                CHECK(dog.GetBag() == restored->GetBag());
                CHECK(dog.GetScore() == restored->GetScore());
            }
        }
    }
}
