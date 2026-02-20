#pragma once

#include <string_view>
#include "catalog/catalog.h"
#include "table/table.h"
#include <memory>

class TableManager {
    public:
    virtual ~TableManager() = default;
    virtual void CreateTable(Catalog& catalog, std::string_view name, const Schema& schema) = 0;
    virtual void DeleteTable(Catalog& catalog, std::string_view name) = 0;
    virtual bool TableExists(std::string_view name) = 0;
    virtual std::unique_ptr<Table> GetTable(std::string_view name) = 0;
};
