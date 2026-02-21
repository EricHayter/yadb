#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include "table/table.h"
#include "common/definitions.h"


struct RelationAttribute {
    std::string name;
    DataType type;
};

using Schema = std::vector<RelationAttribute>;

class TableManager;

class Catalog {
    public:
    Catalog(TableManager& table_manager);
    bool AddTable(std::string_view table_name, const Schema& schema);
    bool RemoveTable(std::string_view table_name);
    std::optional<Schema> GetSchema(std::string_view table_name);

    private:
    TableManager& table_manager_m;
    std::unordered_map<std::string, Schema> table_schemas_m;
    std::unique_ptr<Table> column_catalog_table_m;
    std::unique_ptr<Table> table_catalog_table_m;
    Schema column_catalog_schema_m;
    Schema table_catalog_schema_m;
};
