#pragma once

#include "catalog/catalog.h"
#include "common/definitions.h"
#include "table/table.h"
#include <memory>
#include <string_view>

class TableManager {
public:
    TableManager();

    template<typename... Args>
    bool CreateTable(std::string_view name, TableType type, const Schema& schema, const Args&... args);

    template<typename... Args>
    std::shared_ptr<Table> GetTable(std::string_view name, const Args&... args) const;

    bool DeleteTable(std::string_view name);

    bool TableExists(std::string_view name) const;
private:
    Catalog catalog_m;
};
