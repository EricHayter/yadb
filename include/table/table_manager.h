#pragma once

#include "catalog/catalog.h"
#include "common/definitions.h"
#include "table/table.h"
#include "table/table_factory_interface.h"
#include <memory>
#include <string_view>
#include <unordered_map>

class TableManager {
public:
    explicit TableManager(TableType default_storage_engine = TableType::Disk);

    bool CreateTable(std::string_view name, TableType type, const Schema& schema);
    std::shared_ptr<Table> GetTable(std::string_view name) const;
    bool DeleteTable(std::string_view name);
    bool TableExists(std::string_view name) const;

private:
    ITableFactory& GetFactory(TableType type);

    std::unordered_map<TableType, std::shared_ptr<ITableFactory>> factories_m;
    std::unique_ptr<Catalog> catalog_m;
};
