#pragma once

#include "common/definitions.h"
#include "table/table.h"
#include <memory>
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

    Catalog(std::shared_ptr<Table> table_catalog, std::shared_ptr<Table> column_catalog);
    bool AddTable(std::string_view table_name, TableType table_type, const Schema& schema);
    bool RemoveTable(std::string_view table_name);
    bool TableExists(std::string_view table_name) const;
    Schema GetSchema(std::string_view table_name) const;
    TableType GetTableType(std::string_view table_name) const;

    static const Schema table_catalog_schema;
    static const Schema column_catalog_schema;

private:
    void LoadTableSchemas();
    void LoadColumnSchemas();

    std::unordered_map<std::string, TableInfo> table_info_m;
    std::shared_ptr<Table> table_catalog_table_m;
    std::shared_ptr<Table> column_catalog_table_m;
};
