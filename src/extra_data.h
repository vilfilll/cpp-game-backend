#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include "model.h"

namespace extra_data {

namespace json = boost::json;

class MapExtraData {
public:
    void Add(const model::Map::Id& id, json::array loot_types) {
        data_.emplace(id, std::move(loot_types));
    }

    const json::array& Get(const model::Map::Id& id) const {
        static const json::array empty;
        if (auto it = data_.find(id); it != data_.end()) {
            return it->second;
        }
        return empty;
    }

private:
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    std::unordered_map<model::Map::Id, json::array, MapIdHasher> data_;
};

} // namespace extra_data
