#include "json_loader.h"
#include <fstream>
#include <cmath>
#include <boost/json.hpp>

namespace json = boost::json;
using namespace model;

namespace json_loader {

static Road LoadRoad(const json::object& road_obj) {
    int x0 = road_obj.at("x0").as_int64();
    int y0 = road_obj.at("y0").as_int64();

    if (road_obj.contains("x1")) {
        int x1 = road_obj.at("x1").as_int64();
        return Road{Road::HORIZONTAL, {x0, y0}, x1};
    } else {
        int y1 = road_obj.at("y1").as_int64();
        return Road{Road::VERTICAL, {x0, y0}, y1};
    }
}

static Building LoadBuilding(const json::object& building_obj) {
    return Building(Rectangle{
        {static_cast<Coord>(building_obj.at("x").as_int64()),
         static_cast<Coord>(building_obj.at("y").as_int64())},
        {static_cast<Dimension>(building_obj.at("w").as_int64()),
         static_cast<Dimension>(building_obj.at("h").as_int64())}
    });
}

static Office LoadOffice(const json::object& office_obj) {
    return Office{
        Office::Id(office_obj.at("id").as_string().c_str()),
        {
            static_cast<model::Coord>(office_obj.at("x").as_int64()), 
            static_cast<model::Coord>(office_obj.at("y").as_int64())
        },
        {
            static_cast<model::Dimension>(office_obj.at("offsetX").as_int64()), 
            static_cast<model::Dimension>(office_obj.at("offsetY").as_int64())
        }
    };
}


static Map LoadMap(const json::object& map_obj, double default_dog_speed) {
    double dog_speed = default_dog_speed;
    if (map_obj.contains("dogSpeed")) {
        dog_speed = map_obj.at("dogSpeed").as_double();
    }
    Map map(
        Map::Id(map_obj.at("id").as_string().c_str()),
        map_obj.at("name").as_string().c_str(),
        dog_speed
    );

    if (map_obj.contains("roads")) {
        for (const auto& road_val : map_obj.at("roads").as_array()) {
            map.AddRoad(LoadRoad(road_val.as_object()));
        }
    }

    if (map_obj.contains("buildings")) {
        for (const auto& b : map_obj.at("buildings").as_array()) {
            map.AddBuilding(LoadBuilding(b.as_object()));
        }
    }

    if (map_obj.contains("offices")) {
        for (const auto& office_val : map_obj.at("offices").as_array()) {
            map.AddOffice(LoadOffice(office_val.as_object()));
        }
    }

    if (map_obj.contains("lootTypes")) {
        const auto& loot_types = map_obj.at("lootTypes").as_array();
        map.SetLootTypesCount(loot_types.size());
        for (const auto& lt_val : loot_types) {
            const auto& lt_obj = lt_val.as_object();
            int value = 0;
            if (lt_obj.contains("value")) {
                value = static_cast<int>(lt_obj.at("value").as_int64());
            }
            map.AddLootTypeValue(value);
        }
    }

    if (map_obj.contains("bagCapacity")) {
        map.SetBagCapacity(static_cast<size_t>(map_obj.at("bagCapacity").as_int64()));
    }

    return map;
}

GameData LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    json::value json_data;

    try {
        json_data = json::parse(std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        ));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON file " +
                             json_path.string() + ": " + e.what());
    }

    GameData result;

    auto& game = result.game;
    auto& extra = result.extra_data;

    const auto& root = json_data.as_object();

    if (root.contains("lootGeneratorConfig")) {
        const auto& cfg = root.at("lootGeneratorConfig").as_object();

        double period_sec = cfg.at("period").as_double();
        double probability = cfg.at("probability").as_double();

        game.SetLootGeneratorConfig(
            std::chrono::milliseconds(static_cast<int>(period_sec * 1000)),
            probability
        );
    }

    if (root.contains("defaultBagCapacity")) {
        game.SetDefaultBagCapacity(static_cast<size_t>(root.at("defaultBagCapacity").as_int64()));
    }

    if (root.contains("dogRetirementTime")) {
        const double v = root.at("dogRetirementTime").as_double();
        if (v > 0.0 && std::isfinite(v)) {
            result.dog_retirement_time_sec = v;
        }
    }

    double default_dog_speed = 1.0;
    if (root.contains("defaultDogSpeed")) {
        default_dog_speed = json_data.as_object().at("defaultDogSpeed").as_double();
    }

    const auto& maps = root.at("maps").as_array();

    for (const auto& map_val : maps) {
        const auto& map_obj = map_val.as_object();

        auto map = LoadMap(map_obj, default_dog_speed);

        if (map_obj.contains("lootTypes")) {
            extra.Add(map.GetId(), map_obj.at("lootTypes").as_array());
        }

        game.AddMap(std::move(map));
    }

    return result;
}

}
