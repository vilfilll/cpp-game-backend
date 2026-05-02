#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>

#include <thread>
#include <filesystem>
#include <fstream>
#include <optional>
#include <functional>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <algorithm>

#include <pqxx/pqxx>

#include "connection_pool.h"
#include "retirement_db.h"

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logging.h"
#include "logging_request_handler.h"
#include "serializing_listener.h"

using namespace std::literals;
namespace net = boost::asio;
namespace json = boost::json;
namespace po = boost::program_options;
namespace sys = boost::system;

namespace {

// Запускает fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);

    while (--n) {
        workers.emplace_back(fn);
    }

    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
            ("config-file,c", po::value<std::string>()->value_name("file")->required(), "set config file path")
            ("www-root,w", po::value<std::string>()->value_name("dir")->required(), "set static files root")
            ("randomize-spawn-points", "spawn dogs at random positions")
            ("state-file", po::value<std::string>()->value_name("path"), "path to persistent state file")
            ("save-state-period", po::value<int>()->value_name("milliseconds"),
                "autosave interval in game time (requires --state-file)");

        po::variables_map vm;
        try {
            po::store(po::parse_command_line(argc, argv, desc), vm);

            if (vm.count("help")) {
                std::cout << desc << std::endl;
                return EXIT_SUCCESS;
            }

            po::notify(vm);
        } catch (const po::error& e) {
            std::cerr << e.what() << std::endl;
            std::cout << desc << std::endl;
            return EXIT_FAILURE;
        }

        const auto config_path = vm["config-file"].as<std::string>();
        const auto www_root = vm["www-root"].as<std::string>();
        const bool randomize_spawn = vm.count("randomize-spawn-points") != 0;

        std::optional<std::chrono::milliseconds> tick_period;
        if (vm.count("tick-period")) {
            const auto period_value = vm["tick-period"].as<int>();
            if (period_value <= 0) {
                std::cerr << "tick-period must be positive"sv << std::endl;
                return EXIT_FAILURE;
            }
            tick_period = std::chrono::milliseconds{period_value};
        }

        InitLogging();

        auto game_data = json_loader::LoadGame(config_path);

        model::Game& game = game_data.game;
        game.SetRandomSpawnMode(randomize_spawn);

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());

        std::shared_ptr<db::RetirementDatabase> retirement_db;
        if (const char* db_url = std::getenv("GAME_DB_URL")) {
            const auto pool_size = std::max(num_threads, 4u);
            auto pool = std::make_shared<db::ConnectionPool>(
                pool_size,
                [db_url] {
                    return std::make_shared<pqxx::connection>(db_url);
                });
            retirement_db = std::make_shared<db::RetirementDatabase>(std::move(pool));
            retirement_db->EnsureSchema();
        }

        app::Application app{game, std::move(game_data.extra_data), game_data.dog_retirement_time_sec,
                             std::move(retirement_db)};

        std::optional<std::chrono::milliseconds> save_state_period;
        if (vm.count("state-file")) {
            const auto state_path = std::filesystem::path{vm["state-file"].as<std::string>()};
            if (vm.count("save-state-period")) {
                const auto period_value = vm["save-state-period"].as<int>();
                if (period_value <= 0) {
                    std::cerr << "save-state-period must be positive"sv << std::endl;
                    return EXIT_FAILURE;
                }
                save_state_period = std::chrono::milliseconds{period_value};
            }
            if (std::filesystem::exists(state_path)) {
                std::ifstream in(state_path);
                if (!in) {
                    BOOST_LOG_TRIVIAL(error) << "Cannot open state file for reading";
                    return EXIT_FAILURE;
                }
                try {
                    app.LoadState(in);
                } catch (const std::exception& ex) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to restore state: " << ex.what();
                    return EXIT_FAILURE;
                }
            }
            auto listener = std::make_shared<infra::SerializingListener>(app, state_path, save_state_period);
            app.SetTickListener(listener);
        }

        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int) {
            if (!ec) {
                ioc.stop();
            }
        });

        std::filesystem::path static_root = std::filesystem::absolute(www_root);

        const bool tick_endpoint_enabled = !tick_period.has_value();

        http_handler::RequestHandler handler{app, static_root, api_strand, tick_endpoint_enabled};

        LoggingRequestHandler<http_handler::RequestHandler> logging_handler(handler);

        http_server::ServeHttp(ioc, {address, port}, 
            [&logging_handler](auto&& req, auto&& send, auto&& client_ip) {
                logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), std::forward<decltype(client_ip)>(client_ip));
            });

        std::shared_ptr<app::Ticker> ticker;
        if (tick_period) {
            ticker = std::make_shared<app::Ticker>(api_strand, *tick_period,
                [&app](std::chrono::milliseconds delta) { app.Tick(delta); });
            ticker->Start();
        }

        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(additional_data, start_data)
            << "server started";

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        app.OnShutdownSave();

        json::object exit_data;
        exit_data["code"] = 0;

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(additional_data, exit_data)
            << "server exited";

        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        try {
            InitLogging();

            json::object exit_data;
            exit_data["code"] = EXIT_FAILURE;
            exit_data["exception"] = ex.what();

            BOOST_LOG_TRIVIAL(error)
                << logging::add_value(additional_data, exit_data)
                << "server exited";
        } catch (...) {
            
        }

        return EXIT_FAILURE;
    }
}
