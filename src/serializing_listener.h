#pragma once

#include "application_listener.h"

#include <chrono>
#include <filesystem>
#include <optional>

namespace app {
class Application;
}

namespace infra {

class SerializingListener : public app::ApplicationListener {
public:
    SerializingListener(app::Application& app, std::filesystem::path state_file,
                        std::optional<std::chrono::milliseconds> save_period);

    void OnTick(std::chrono::milliseconds delta) override;
    void OnShutdownSave() override;

private:
    void SaveToFile();

    app::Application& app_;
    std::filesystem::path state_file_;
    std::optional<std::chrono::milliseconds> save_period_;
    std::chrono::milliseconds accumulated_{0};
};

}  // namespace infra
