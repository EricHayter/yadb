#pragma once

#include "common/definitions.h"
#include "table/table.h"
#include "table/table_handle.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class TableManager;
class ITableFactory;

class Catalog {
public:
    struct TableInfo {
        TableType type;
        Schema schema;
    };

    Catalog(ITableFactory& table_factory);
    bool AddTable(std::string_view table_name, TableType table_type, const Schema& schema);
    bool RemoveTable(std::string_view table_name);
    bool TableExists(std::string_view table_name) const;
    Schema GetSchema(std::string_view table_name) const;
    TableType GetTableType(std::string_view table_name) const;

    static const Schema table_catalog_schema;
    static const Schema column_catalog_schema;

    // Catalog table name constants
    static constexpr std::string_view TABLE_CATALOG_TABLE_NAME = "table_catalog";
    static constexpr std::string_view COLUMN_CATALOG_TABLE_NAME = "column_catalog";

    static void InitializeTableCatalogTable(TableHandle& table_catalog);
    static void InitializeColumnCatalogTable(TableHandle& column_catalog);

private:
    // helper function in the catalog constructor that builds the required
    // tables for the catalog (if they don't already exist) with the provided
    // table factory.
    void InitTables(ITableFactory& table_factory);

    // populates the list of tables in table_info_m (with no schema info!)
    void LoadTableSchemas();

    // populates the schemas for each of the tables added to table_info_m
    // created by LoadTableSchemas
    void LoadColumnSchemas();

    // mappings from table names to infromation about tables, e.g. table type
    // and schema
    std::unordered_map<std::string, TableInfo> table_info_m;

    std::optional<TableHandle> table_catalog_table_m;
    std::optional<TableHandle> column_catalog_table_m;
};
