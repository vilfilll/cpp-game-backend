#pragma once

#include "application_listener.h"
#include "extra_data.h"
#include "model.h"
#include "retirement_db.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <iosfwd>
#include <functional>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <cmath>

namespace net = boost::asio;
namespace sys = boost::system;

namespace app {

class Player {
public:
    using Id = std::size_t;

    Player(Id id, model::GameSession& session, model::Dog& dog) noexcept
        : id_(id)
        , session_(session)
        , dog_(dog) {
    }

    Id GetId() const noexcept {
        return id_;
    }

    model::GameSession& GetSession() const noexcept {
        return session_.get();
    }

    model::Dog& GetDog() const noexcept {
        return dog_.get();
    }

private:
    Id id_;
    std::reference_wrapper<model::GameSession> session_;
    std::reference_wrapper<model::Dog> dog_;
};

class Players {
public:
    Player& Add(model::Dog& dog, model::GameSession& session) {
        auto player = std::make_unique<Player>(next_player_id_++, session, dog);
        Player& ref = *player;
        players_.push_back(std::move(player));
        return ref;
    }

    Player* FindByDogIdAndMapId(const model::Dog::Id& dog_id, const model::Map::Id& map_id) noexcept {
        auto it = std::find_if(players_.begin(), players_.end(),
            [&](const std::unique_ptr<Player>& p) {
                return p &&
                    p->GetDog().GetId() == dog_id &&
                    p->GetSession().GetMap().GetId() == map_id;
            });

        return it != players_.end() ? it->get() : nullptr;
    }

    void Clear() {
        players_.clear();
        next_player_id_ = 0;
    }

    Player& RestorePlayer(Player::Id id, model::Dog& dog, model::GameSession& session) {
        auto player = std::make_unique<Player>(id, session, dog);
        Player& ref = *player;
        players_.push_back(std::move(player));
        return ref;
    }

    Player::Id GetNextPlayerId() const noexcept {
        return next_player_id_;
    }

    void SetNextPlayerId(Player::Id id) noexcept {
        next_player_id_ = id;
    }

    Player* FindById(Player::Id id) noexcept {
        for (auto& up : players_) {
            if (up && up->GetId() == id) {
                return up.get();
            }
        }
        return nullptr;
    }

    void RemovePlayer(Player* player) {
        auto it = std::find_if(players_.begin(), players_.end(),
                               [player](const std::unique_ptr<Player>& up) {
                                   return up.get() == player;
                               });
        if (it != players_.end()) {
            players_.erase(it);
        }
    }

    std::vector<const Player*> GetPlayersInSession(const model::GameSession& session) const {
        std::vector<const Player*> result;

        for (const auto& p : players_) {
            if (p && &p->GetSession() == &session) {
                result.push_back(p.get());
            }
        }

        return result;
    }

    std::vector<const Player*> GetAllPlayers() const {
        std::vector<const Player*> result;
        for (const auto& p : players_) {
            if (p) {
                result.push_back(p.get());
            }
        }
        return result;
    }

    std::vector<Player*> GetAllMutablePlayers() {
        std::vector<Player*> result;
        for (auto& p : players_) {
            if (p) {
                result.push_back(p.get());
            }
        }
        return result;
    }

private:
    Player::Id next_player_id_ = 0;
    std::vector<std::unique_ptr<Player>> players_;
};

struct TokenTag {};
using Token = util::Tagged<std::string, TokenTag>;

class PlayerTokens {
public:
    PlayerTokens()
        : generator1_(std::random_device{}())
        , generator2_(std::random_device{}()) {
    }

    Token AddPlayer(Player& player) {
        constexpr int kMaxAttempts = 64;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            auto token = GenerateToken();
            if (auto [it, inserted] = token_to_player_.emplace(token, &player); inserted) {
                return token;
            }
        }
        throw std::runtime_error("failed to generate unique auth token");
    }

    Player* FindPlayerByToken(std::string_view token) const {
        auto it = token_to_player_.find(Token{std::string(token)});
        if (it == token_to_player_.end()) {
            return nullptr;
        }
        return it->second;
    }

    void Clear() {
        token_to_player_.clear();
    }

    void RestoreMapping(const std::string& token_str, Player& player) {
        Token token{token_str};
        if (!token_to_player_.emplace(token, &player).second) {
            throw std::runtime_error("duplicate token in state file");
        }
    }

    void RemovePlayerMapping(const Player* player) noexcept {
        const auto it = std::find_if(token_to_player_.begin(), token_to_player_.end(),
                                     [player](const auto& e) { return e.second == player; });
        if (it != token_to_player_.end()) {
            token_to_player_.erase(it);
        }
    }

    std::optional<std::string> FindTokenForPlayer(const Player* player) const {
        if (!player) {
            return std::nullopt;
        }
        const auto it = std::find_if(token_to_player_.begin(), token_to_player_.end(),
                                      [player](const auto& e) { return e.second == player; });
        if (it != token_to_player_.end()) {
            return std::string{*it->first};
        }
        return std::nullopt;
    }

private:
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;

    Token GenerateToken() {
        const auto x = generator1_();
        const auto y = generator2_();

        std::ostringstream oss;
        oss << std::hex << std::nouppercase << std::setfill('0')
            << std::setw(16) << static_cast<std::uint64_t>(x)
            << std::setw(16) << static_cast<std::uint64_t>(y);
        return Token{oss.str()};
    }

    struct TokenHasher {
        std::size_t operator()(const Token& t) const noexcept {
            return std::hash<std::string>{}(*t);
        }
    };

    std::unordered_map<Token, Player*, TokenHasher> token_to_player_;
};

struct JoinResult {
    Player::Id player_id;
    std::string token;
};

struct PlayerInfo {
    Player::Id id;
    std::string_view name;
    model::Position position;
    model::Speed speed;
    model::Direction direction;
    std::vector<model::BagItem> bag;
    int score;
};

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] {
            self->ScheduleTick();
        });
    }

private:
    using Clock = std::chrono::steady_clock;

    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    Clock::time_point last_tick_;
};

class Application {
public:
    Application(model::Game& game, extra_data::MapExtraData extra, double dog_retirement_time_sec,
                  std::shared_ptr<db::RetirementDatabase> retirement_db)
        : game_(game)
        , extra_data_(std::move(extra))
        , retirement_duration_ms_(MakeRetirementDurationMs(dog_retirement_time_sec))
        , retirement_db_(std::move(retirement_db)) {
    }

    const extra_data::MapExtraData& GetExtraData() const {
        return extra_data_;
    }

    const model::Game::Maps& ListMaps() const {
        return game_.GetMaps();
    }

    const model::Map* FindMap(const model::Map::Id& id) const {
        return game_.FindMap(id);
    }

    JoinResult JoinGame(const model::Map::Id& map_id, std::string user_name);

    std::vector<PlayerInfo> GetPlayersByToken(std::string_view token) const;

    void SetPlayerAction(std::string_view token, std::string_view move);

    void Tick(std::chrono::milliseconds delta);

    void OnShutdownSave();

    void SetTickListener(std::shared_ptr<ApplicationListener> listener) {
        tick_listener_ = std::move(listener);
    }

    void SaveState(std::ostream& os) const;

    void LoadState(std::istream& is);

    std::vector<model::Loot> GetLostObjects(std::string_view token) const;

    std::vector<db::RetiredPlayerRow> ListRetiredRecords(int start, int max_items) const;

private:
    static std::chrono::milliseconds MakeRetirementDurationMs(double dog_retirement_time_sec) noexcept;

    struct RetirementState {
        std::chrono::milliseconds join_time_ms{0};
        std::optional<std::chrono::milliseconds> idle_since_ms;
    };

    model::Game& game_;

    extra_data::MapExtraData extra_data_;

    model::GameSession& GetOrCreateSession(const model::Map::Id& map_id);

    Players players_;
    PlayerTokens tokens_;
    std::mt19937 random_engine_{std::random_device{}()};

    std::shared_ptr<ApplicationListener> tick_listener_;

    std::chrono::milliseconds retirement_duration_ms_{};
    std::shared_ptr<db::RetirementDatabase> retirement_db_;
    std::chrono::milliseconds game_elapsed_{0};
    std::unordered_map<Player::Id, RetirementState> retirement_;

    double GameTimeSec() const noexcept {
        return std::chrono::duration<double>(game_elapsed_).count();
    }

    void ProcessRetirements();
    void RetirePlayer(Player& player);
    void InitRetirementForAllPlayers();
};

}
