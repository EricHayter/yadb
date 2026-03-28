#pragma once

#include "common/definitions.h"
#include "table/table.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class TableManager;

class Catalog {
public:
    struct TableInfo {
        TableType type;
        Schema schema;
    };

    Catalog(TableManager& table_manager);
    bool AddTable(std::string_view table_name, TableType table_type, const Schema& schema);
    bool RemoveTable(std::string_view table_name);
    bool TableExists(std::string_view table_name) const;
    std::optional<Schema> GetSchema(std::string_view table_name) const;
    std::optional<TableType> GetTableType(std::string_view table_name) const;

private:
    void InitializeTableCatalog();
    void InitializeColumnCatalog();
    void LoadTableSchemas();
    void LoadColumnSchemas();

    TableManager& table_manager_m;
    std::unordered_map<std::string, TableInfo> table_info_m;
    std::shared_ptr<Table> column_catalog_table_m;
    std::shared_ptr<Table> table_catalog_table_m;
    const Schema column_catalog_schema_m;
    const Schema table_catalog_schema_m;
};
