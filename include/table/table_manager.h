#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

// TODO this is likely going to require more information but it's good enough for now.
struct RelationAttribute {
    enum class Type : std::uint8_t {
        INTEGER,    // std::int32
        TEXT,       // std::string
    };

    std::string name;
    Type type;
};

// FWD declaration of catalog class still need to make interface for this.
class Catalog {};

using Schema = std::vector<RelationAttribute>;

class TableManager {
    public:
    virtual ~TableManager() = default;
    virtual void CreateTable(Catalog& catalog, std::string_view name, const Schema& schema) = 0;
    virtual void DeleteTable(Catalog& catalog, std::string_view name) = 0;
};
