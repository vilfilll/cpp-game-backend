#pragma once

#include <chrono>
#include <filesystem>

#include "model.h"
#include "extra_data.h"

namespace json_loader {

struct GameData {
    model::Game game;
    extra_data::MapExtraData extra_data;
    double dog_retirement_time_sec{60.0};
};

GameData LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
