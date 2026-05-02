#pragma once

#include "connection_pool.h"

#include <memory>
#include <string>
#include <vector>

namespace db {

struct RetiredPlayerRow {
    std::string name;
    int score;
    double play_time_sec;
};

class RetirementDatabase {
public:
    explicit RetirementDatabase(std::shared_ptr<ConnectionPool> pool);

    void EnsureSchema();

    void InsertRetired(const std::string& name, int score, double play_time_sec);

    std::vector<RetiredPlayerRow> FetchRecords(int start, int max_items);

private:
    std::shared_ptr<ConnectionPool> pool_;
};

}  // namespace db
