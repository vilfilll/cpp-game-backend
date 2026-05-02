#include "serializing_listener.h"

#include "application.h"
#include "logging.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace infra {

SerializingListener::SerializingListener(app::Application& app, std::filesystem::path state_file,
                                         std::optional<std::chrono::milliseconds> save_period)
    : app_(app)
    , state_file_(std::move(state_file))
    , save_period_(save_period) {
}

void SerializingListener::OnTick(std::chrono::milliseconds delta) {
    if (delta.count() <= 0) {
        return;
    }
    accumulated_ += delta;
    if (save_period_ && accumulated_ >= *save_period_) {
        SaveToFile();
        accumulated_ = std::chrono::milliseconds{0};
    }
}

void SerializingListener::OnShutdownSave() {
    SaveToFile();
}

void SerializingListener::SaveToFile() {
    std::filesystem::path tmp = state_file_;
    tmp += ".tmp";

    {
        std::ofstream out(tmp, std::ios::out | std::ios::trunc);
        if (!out) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open temporary state file for writing";
            return;
        }
        try {
            app_.SaveState(out);
        } catch (const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "State serialization failed: " << ex.what();
            out.close();
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec);
            return;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp, state_file_, ec);
    if (ec) {
        std::error_code rm_ec;
        std::filesystem::remove(state_file_, rm_ec);
        ec.clear();
        std::filesystem::rename(tmp, state_file_, ec);
    }
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Failed to rename state file: " << ec.message();
    }
}

}  // namespace infra
