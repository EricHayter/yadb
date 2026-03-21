#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <memory>
#include "common/definitions.h"
#include "table/table.h"

class TableManager {
public:
    TableManager() = default;

    bool CreateTable(std::string_view name, TableType type, const Schema& schema);
    bool DeleteTable(std::string_view name);
    bool TableExists(std::string_view name) const;
    std::shared_ptr<Table> GetTable(std::string_view name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Table>> tables_m;
};
