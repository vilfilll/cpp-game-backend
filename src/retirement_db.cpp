#include "retirement_db.h"

#include <pqxx/pqxx>

namespace db {

namespace {

constexpr std::string_view kCreateTable = R"SQL(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name TEXT NOT NULL,
    score INTEGER NOT NULL,
    play_time_sec DOUBLE PRECISION NOT NULL
);
)SQL";

constexpr std::string_view kCreateIndex = R"SQL(
CREATE INDEX IF NOT EXISTS retired_players_score_time_name_idx
ON retired_players (score DESC, play_time_sec ASC, name ASC);
)SQL";

}  // namespace

RetirementDatabase::RetirementDatabase(std::shared_ptr<ConnectionPool> pool)
    : pool_{std::move(pool)} {
}

void RetirementDatabase::EnsureSchema() {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    work.exec(std::string{kCreateTable});
    work.exec(std::string{kCreateIndex});
    work.commit();
}

void RetirementDatabase::InsertRetired(const std::string& name, int score, double play_time_sec) {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    work.exec_params(
        "INSERT INTO retired_players (name, score, play_time_sec) VALUES ($1, $2, $3)",
        name,
        score,
        play_time_sec);
    work.commit();
}

std::vector<RetiredPlayerRow> RetirementDatabase::FetchRecords(int start, int max_items) {
    auto conn = pool_->GetConnection();
    pqxx::work work{*conn};
    const auto rows = work.exec_params(
        "SELECT name, score, play_time_sec FROM retired_players "
        "ORDER BY score DESC, play_time_sec ASC, name ASC "
        "LIMIT $1 OFFSET $2",
        max_items,
        start);
    work.commit();

    std::vector<RetiredPlayerRow> out;
    out.reserve(static_cast<size_t>(rows.size()));
    for (const auto& row : rows) {
        out.push_back(RetiredPlayerRow{
            row[0].as<std::string>(),
            row[1].as<int>(),
            row[2].as<double>()});
    }
    return out;
}

}  // namespace db
