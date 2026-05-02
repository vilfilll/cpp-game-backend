#pragma once
#include "application.h"
#include "extra_data.h"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

namespace json_keys {

constexpr boost::json::string_view X = "x";
constexpr boost::json::string_view Y = "y";
constexpr boost::json::string_view W = "w";
constexpr boost::json::string_view H = "h";

constexpr boost::json::string_view X0 = "x0";
constexpr boost::json::string_view Y0 = "y0";
constexpr boost::json::string_view X1 = "x1";
constexpr boost::json::string_view Y1 = "y1";

constexpr boost::json::string_view ID = "id";
constexpr boost::json::string_view NAME = "name";

constexpr boost::json::string_view ROADS = "roads";
constexpr boost::json::string_view BUILDINGS = "buildings";
constexpr boost::json::string_view OFFICES = "offices";

constexpr boost::json::string_view OFFSET_X = "offsetX";
constexpr boost::json::string_view OFFSET_Y = "offsetY";

constexpr boost::json::string_view CODE = "code";
constexpr boost::json::string_view MESSAGE = "message";

constexpr boost::json::string_view LOOT_TYPES = "lootTypes";

}

class ApiEndpoints {
public:
    static constexpr std::string_view MAPS_ENDPOINT = "/api/v1/maps";
    static constexpr std::string_view MAP_ENDPOINT_PREFIX = "/api/v1/maps/";
    static constexpr std::string_view JOIN_ENDPOINT = "/api/v1/game/join";
    static constexpr std::string_view PLAYERS_ENDPOINT = "/api/v1/game/players";
    static constexpr std::string_view STATE_ENDPOINT = "/api/v1/game/state";
    static constexpr std::string_view PLAYER_ACTION_ENDPOINT = "/api/v1/game/player/action";
    static constexpr std::string_view TICK_ENDPOINT = "/api/v1/game/tick";
    static constexpr std::string_view RECORDS_ENDPOINT = "/api/v1/game/records";

    static bool IsMapsEndpoint(std::string_view target) {
        return target == MAPS_ENDPOINT;
    }
    static bool IsMapEndpoint(std::string_view target) {
        return target.starts_with(MAP_ENDPOINT_PREFIX);
    }
    static bool IsJoinEndpoint(std::string_view target) {
        return target == JOIN_ENDPOINT;
    }
    static bool IsPlayersEndpoint(std::string_view target) {
        return target == PLAYERS_ENDPOINT;
    }
    static bool IsStateEndpoint(std::string_view target) {
        return target == STATE_ENDPOINT;
    }
    static bool IsPlayerActionEndpoint(std::string_view target) {
        return target == PLAYER_ACTION_ENDPOINT;
    }
    static bool IsTickEndpoint(std::string_view target) {
        return target == TICK_ENDPOINT;
    }
    static bool IsRecordsEndpoint(std::string_view target) {
        return target == RECORDS_ENDPOINT;
    }
};

class ApiHandler {
public:
    explicit ApiHandler(app::Application& app, bool tick_enabled = true) noexcept
        : app_(app)
        , tick_enabled_(tick_enabled) {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string full_target = std::string(req.target());
        std::string_view path_sv = full_target;
        if (const auto q = full_target.find('?'); q != std::string::npos) {
            path_sv = std::string_view(full_target.data(), q);
        }
        std::string path(path_sv);
        while (path.size() > 1 && path.back() == '/') {
            path.pop_back();
        }

        if (!path.starts_with("/api/v1/")) {
            send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", req));
            return;
        }

        if (ApiEndpoints::IsJoinEndpoint(path)) {
            return HandleJoin(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsPlayersEndpoint(path)) {
            return HandlePlayers(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsStateEndpoint(path)) {
            return HandleGameState(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsPlayerActionEndpoint(path)) {
            return HandlePlayerAction(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsTickEndpoint(path) && tick_enabled_) {
            return HandleTick(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsRecordsEndpoint(path)) {
            return HandleRecords(req, std::forward<Send>(send), full_target);
        }
        if (ApiEndpoints::IsMapsEndpoint(path)) {
            return HandleMaps(req, std::forward<Send>(send));
        }
        if (ApiEndpoints::IsMapEndpoint(path)) {
            return HandleMap(req, std::forward<Send>(send));
        }

        if (ApiEndpoints::IsTickEndpoint(path) && !tick_enabled_) {
            send(MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", req));
            return;
        }

        send(MakeErrorResponse(http::status::not_found, "badRequest", "Bad request", req));
    }

private:
    app::Application& app_;
    bool tick_enabled_{true};

    static bool ParsePositiveInt(std::string_view s, int& out) {
        if (s.empty()) {
            return false;
        }
        int value = 0;
        for (char ch : s) {
            if (ch < '0' || ch > '9') {
                return false;
            }
            const int d = ch - '0';
            if (value > (std::numeric_limits<int>::max() - d) / 10) {
                return false;
            }
            value = value * 10 + d;
        }
        out = value;
        return true;
    }

    static bool ParseRecordsQuery(std::string_view full_target, int& start, int& max_items) {
        start = 0;
        max_items = 100;
        const auto q = full_target.find('?');
        if (q == std::string_view::npos) {
            return true;
        }
        std::string_view query = full_target.substr(q + 1);
        while (!query.empty()) {
            std::string_view part;
            if (const auto amp = query.find('&'); amp != std::string_view::npos) {
                part = query.substr(0, amp);
                query.remove_prefix(amp + 1);
            } else {
                part = query;
                query = {};
            }
            const auto eq = part.find('=');
            if (eq == std::string_view::npos) {
                continue;
            }
            std::string_view key = part.substr(0, eq);
            std::string_view val = part.substr(eq + 1);
            int parsed = 0;
            if (key == "start") {
                if (!ParsePositiveInt(val, parsed)) {
                    return false;
                }
                start = parsed;
            } else if (key == "maxItems") {
                if (!ParsePositiveInt(val, parsed)) {
                    return false;
                }
                max_items = parsed;
            }
        }
        return true;
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleRecords(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send,
                       std::string_view full_target) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req);
            res.set(http::field::allow, "GET, HEAD");
            send(std::move(res));
            return;
        }

        int start = 0;
        int max_items = 100;
        if (!ParseRecordsQuery(full_target, start, max_items)) {
            send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", req));
            return;
        }
        if (max_items > 100) {
            send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", req));
            return;
        }

        json::array arr;
        for (const auto& row : app_.ListRetiredRecords(start, max_items)) {
            json::object o;
            o["name"] = row.name;
            o["score"] = row.score;
            o["playTime"] = row.play_time_sec;
            arr.push_back(std::move(o));
        }

        auto res = MakeJsonResponse(http::status::ok, json::serialize(arr), req);
        if (req.method() == http::verb::head) {
            res.body().clear();
            res.prepare_payload();
        }
        send(std::move(res));
    }

    template <typename Request>
    auto MakeJsonResponse(http::status status, std::string body, const Request& req) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.body() = std::move(body);
        res.prepare_payload();
        return res;
    }

    template <typename Request>
    auto MakeErrorResponse(http::status status, std::string code, std::string message, const Request& req) {
        json::object obj;
        obj[json_keys::CODE] = code;
        obj[json_keys::MESSAGE] = message;
        return MakeJsonResponse(status, json::serialize(obj), req);
    }

    json::object SerializeRoad(const model::Road& road) {
        json::object obj;
        obj[json_keys::X0] = road.GetStart().x;
        obj[json_keys::Y0] = road.GetStart().y;
        if (road.IsHorizontal()) {
            obj[json_keys::X1] = road.GetEnd().x;
        } else {
            obj[json_keys::Y1] = road.GetEnd().y;
        }
        return obj;
    }

    json::object SerializeBuilding(const model::Building& building) {
        json::object obj;
        auto bounds = building.GetBounds();
        obj[json_keys::X] = bounds.position.x;
        obj[json_keys::Y] = bounds.position.y;
        obj[json_keys::W] = bounds.size.width;
        obj[json_keys::H] = bounds.size.height;
        return obj;
    }

    json::object SerializeOffice(const model::Office& office) {
        json::object obj;
        obj[json_keys::ID] = *office.GetId();
        obj[json_keys::X] = office.GetPosition().x;
        obj[json_keys::Y] = office.GetPosition().y;
        obj[json_keys::OFFSET_X] = office.GetOffset().dx;
        obj[json_keys::OFFSET_Y] = office.GetOffset().dy;
        return obj;
    }

    json::object SerializeMap(const model::Map& map, const extra_data::MapExtraData& extra_data) {
        json::object obj;
        obj[json_keys::ID] = *map.GetId();
        obj[json_keys::NAME] = map.GetName();

        json::array roads;
        for (const auto& road : map.GetRoads()) {
            roads.push_back(SerializeRoad(road));
        }
        obj[json_keys::ROADS] = std::move(roads);

        json::array buildings;
        for (const auto& b : map.GetBuildings()) {
            buildings.push_back(SerializeBuilding(b));
        }
        obj[json_keys::BUILDINGS] = std::move(buildings);

        json::array offices;
        for (const auto& o : map.GetOffices()) {
            offices.push_back(SerializeOffice(o));
        }
        obj[json_keys::OFFICES] = std::move(offices);

        obj[json_keys::LOOT_TYPES] = extra_data.Get(map.GetId());

        return obj;
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMaps(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        json::array maps;
        for (const auto& map : app_.ListMaps()) {
            json::object obj;
            obj[json_keys::ID] = *map.GetId();
            obj[json_keys::NAME] = map.GetName();
            maps.push_back(std::move(obj));
        }
        send(MakeJsonResponse(http::status::ok, json::serialize(maps), req));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMap(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {

            auto res = MakeErrorResponse(
                http::status::method_not_allowed,
                "invalidMethod",
                "Invalid method",
                req
            );

            res.set(http::field::allow, "GET, HEAD");

            send(std::move(res));
            return;
        }
        const std::string full_target = std::string(req.target());
        std::string_view path_sv = full_target;
        if (const auto q = full_target.find('?'); q != std::string::npos) {
            path_sv = std::string_view(full_target.data(), q);
        }
        std::string map_id = std::string(path_sv.substr(std::string(ApiEndpoints::MAP_ENDPOINT_PREFIX).size()));
        const auto* map = app_.FindMap(model::Map::Id(map_id));
        if (!map) {
            send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
            return;
        }
        send(MakeJsonResponse(http::status::ok, json::serialize(SerializeMap(*map, app_.GetExtraData())), req));
    }

    static bool IsValidHexToken(std::string_view token) {
        if (token.size() != 32) {
            return false;
        }
        for (unsigned char ch : token) {
            if (!std::isxdigit(ch)) {
                return false;
            }
        }
        return true;
    }

    static std::string_view TrimHttpWhitespace(std::string_view s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
            s.remove_prefix(1);
        }
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.remove_suffix(1);
        }
        return s;
    }

    static bool CaseInsensitiveStartsWith(std::string_view s, std::string_view prefix) {
        if (s.size() < prefix.size()) {
            return false;
        }
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    template <typename Request>
    std::optional<std::string> TryExtractToken(const Request& req) {
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return std::nullopt;
        }
        std::string_view auth_value = TrimHttpWhitespace(auth_it->value());
        constexpr std::string_view bearer_prefix = "bearer ";
        if (!CaseInsensitiveStartsWith(auth_value, bearer_prefix)) {
            return std::nullopt;
        }
        std::string_view token = auth_value.substr(bearer_prefix.size());
        token = TrimHttpWhitespace(token);
        while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) {
            token.remove_suffix(1);
        }
        if (!IsValidHexToken(token)) {
            return std::nullopt;
        }
        return std::string(token);
    }

    template <typename Body, typename Allocator, typename Send, typename Fn>
    void ExecuteAuthorized(http::request<Body, http::basic_fields<Allocator>>& request, Send&& send, Fn&& action) {
        auto token = TryExtractToken(request);
        if (!token) {
            send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", request));
            return;
        }
        try {
            action(*token);
        } catch (const std::out_of_range&) {
            send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", request));
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req);
            res.set(http::field::allow, "POST");
            if (req.method() == http::verb::head) {
                res.body().clear();
                res.prepare_payload();
            }
            send(std::move(res));
            return;
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || std::string_view(ct_it->value()).find("application/json") == std::string_view::npos) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            return;
        }

        json::value body;
        json::error_code ec;
        body = json::parse(req.body(), ec);

        if (ec) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            return;
        }

        json::object* obj = body.if_object();
        if (!obj) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            return;
        }

        auto* user_name_val = obj->if_contains("userName");
        auto* map_id_val = obj->if_contains("mapId");
        if (!user_name_val || !map_id_val || !user_name_val->is_string() || !map_id_val->is_string()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            return;
        }

        std::string user_name = std::string(user_name_val->as_string());
        std::string map_id = std::string(map_id_val->as_string());

        if (user_name.empty()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", req));
            return;
        }
        if (!app_.FindMap(model::Map::Id(map_id))) {
            send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
            return;
        }

        auto join = app_.JoinGame(model::Map::Id(map_id), std::move(user_name));
        json::object resp;
        resp["authToken"] = join.token;
        resp["playerId"] = join.player_id;
        send(MakeJsonResponse(http::status::ok, json::serialize(resp), req));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayers(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req);
            res.set(http::field::allow, "GET, HEAD");
            send(std::move(res));
            return;
        }

        ExecuteAuthorized(req, send, [this, &req, &send](const std::string& token) {
            json::object resp;
            for (const auto& p : app_.GetPlayersByToken(token)) {
                json::object player_obj;
                player_obj["name"] = std::string(p.name);
                resp[std::to_string(p.id)] = std::move(player_obj);
            }
            auto res = MakeJsonResponse(http::status::ok, json::serialize(resp), req);
            if (req.method() == http::verb::head) {
                res.body().clear();
                res.prepare_payload();
            }
            send(std::move(res));
        });
    }

    static std::string DirectionToString(model::Direction dir) {
        switch (dir) {
            case model::Direction::NORTH:
                return "U";
            case model::Direction::SOUTH:
                return "D";
            case model::Direction::WEST:
                return "L";
            case model::Direction::EAST:
                return "R";
        }
        return "U";
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGameState(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req);
            res.set(http::field::allow, "GET, HEAD");
            send(std::move(res));
            return;
        }

        ExecuteAuthorized(req, send, [this, &req, &send](const std::string& token) {
            json::object players_obj;
            for (const auto& p : app_.GetPlayersByToken(token)) {
                json::object player_obj;

                json::array pos;
                pos.push_back(p.position.x);
                pos.push_back(p.position.y);

                json::array speed;
                speed.push_back(p.speed.x);
                speed.push_back(p.speed.y);

                player_obj["pos"] = std::move(pos);
                player_obj["speed"] = std::move(speed);
                player_obj["dir"] = DirectionToString(p.direction);

                json::array bag;
                for (const auto& item : p.bag) {
                    json::object bag_item;
                    bag_item["id"] = static_cast<std::int64_t>(item.id);
                    bag_item["type"] = item.type;
                    bag.push_back(std::move(bag_item));
                }
                player_obj["bag"] = std::move(bag);
                player_obj["score"] = p.score;

                players_obj[std::to_string(p.id)] = std::move(player_obj);
            }

            json::object lost_objects;

            for (const auto& loot : app_.GetLostObjects(token)) {
                json::object loot_obj;

                loot_obj["type"] = loot.type;

                json::array pos;
                pos.push_back(loot.position.x);
                pos.push_back(loot.position.y);

                loot_obj["pos"] = std::move(pos);

                lost_objects[std::to_string(*loot.id)] = std::move(loot_obj);
            }

            json::object resp;
            resp["players"] = std::move(players_obj);
            resp["lostObjects"] = std::move(lost_objects);

            auto res = MakeJsonResponse(http::status::ok, json::serialize(resp), req);
            if (req.method() == http::verb::head) {
                res.body().clear();
                res.prepare_payload();
            }
            send(std::move(res));
        });
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayerAction(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req);
            res.set(http::field::allow, "POST");
            send(std::move(res));
            return;
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || std::string_view(ct_it->value()).find("application/json") == std::string_view::npos) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
            return;
        }

        json::value body;
        json::error_code ec;
        body = json::parse(req.body(), ec);

        if (ec) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            return;
        }

        json::object* obj = body.if_object();
        if (!obj) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            return;
        }

        auto* move_val = obj->if_contains("move");
        if (!move_val) {
            move_val = obj->if_contains("Move");
        }
        if (!move_val || !move_val->is_string()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            return;
        }

        std::string move = std::string(move_val->as_string());
        if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            return;
        }

        ExecuteAuthorized(req, send, [this, &req, &send, move = std::move(move)](const std::string& token) {
            try {
                app_.SetPlayerAction(token, move);
            } catch (const std::invalid_argument&) {
                send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
                return;
            }
            send(MakeJsonResponse(http::status::ok, "{}", req));
        });
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleTick(http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req);
            res.set(http::field::allow, "POST");
            send(std::move(res));
            return;
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || std::string_view(ct_it->value()).find("application/json") == std::string_view::npos) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
            return;
        }

        json::value body;
        json::error_code ec;
        body = json::parse(req.body(), ec);

        if (ec) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            return;
        }

        json::object* obj = body.if_object();
        if (!obj) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            return;
        }

        auto* td_val = obj->if_contains("timeDelta");
        if (!td_val || !(td_val->is_int64() || td_val->is_uint64())) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid timeDelta value", req));
            return;
        }

        std::int64_t td = 0;
        if (td_val->is_int64()) {
            td = td_val->as_int64();
        } else {
            const auto u = td_val->as_uint64();
            if (u > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid timeDelta value", req));
                return;
            }
            td = static_cast<std::int64_t>(u);
        }

        if (td < 0) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid timeDelta value", req));
            return;
        }

        app_.Tick(std::chrono::milliseconds{td});
        send(MakeJsonResponse(http::status::ok, "{}", req));
    }
};

}  // namespace http_handler

