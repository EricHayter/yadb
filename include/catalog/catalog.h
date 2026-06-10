#pragma once

#include "common/definitions.h"
#include "table/table_handle.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class ITableFactory;

class Catalog {
public:
    struct TableInfo {
        TableType type;
        Schema schema;
    };

    explicit Catalog(ITableFactory& factory);
    bool AddTable(std::string_view table_name, TableType table_type, const Schema& schema);
    bool RemoveTable(std::string_view table_name);
    bool TableExists(std::string_view table_name) const;
    Schema GetSchema(std::string_view table_name) const;
    TableType GetTableType(std::string_view table_name) const;

    static const Schema table_catalog_schema;
    static const Schema column_catalog_schema;

    static constexpr std::string_view TABLE_CATALOG_TABLE_NAME = "table_catalog";
    static constexpr std::string_view COLUMN_CATALOG_TABLE_NAME = "column_catalog";

private:
    void CreateCatalogTables(ITableFactory& factory);
    void InitializeTableCatalogTable();
    void InitializeColumnCatalogTable();
    void LoadTableSchemas();
    void LoadColumnSchemas();

    std::unordered_map<std::string, TableInfo> table_info_m;

    std::optional<TableHandle> table_catalog_table_m;
    std::optional<TableHandle> column_catalog_table_m;
};
