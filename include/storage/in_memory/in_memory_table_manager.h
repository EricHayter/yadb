#pragma once

#include "table/table_manager.h"
#include "catalog/catalog.h"
#include <unordered_map>
#include <memory>
#include <string>

class InMemoryTableManager : public TableManager {
public:
    InMemoryTableManager() = default;
    ~InMemoryTableManager() override = default;

    bool CreateTable(std::string_view name, const Schema& schema) override;
    bool DeleteTable(std::string_view name) override;
    bool TableExists(std::string_view name) override;
    std::unique_ptr<Table> GetTable(std::string_view name) override;

private:
    // Store table schemas (catalog-like metadata)
    std::unordered_map<std::string, Schema> table_schemas_m;

    // Store actual table data in memory
    std::unordered_map<std::string, std::shared_ptr<class InMemoryTable>> tables_m;
};
