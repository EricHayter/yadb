#pragma once

#include "table/table_factory_interface.h"
#include "catalog/catalog.h"
#include "storage/ephemeral/ephemeral_table.h"
#include <memory>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>

class EphemeralTableFactory : public ITableFactory {
public:
    EphemeralTableFactory();
    void SetCatalog(Catalog& catalog) override;
    bool TableExists(std::string_view table_name) const override;
    bool CreateTable(std::string_view table_name, const Schema& schema) override;
    std::shared_ptr<Table> GetTable(std::string_view table_name) override;
    bool DeleteTable(std::string_view table_name) override;

private:
    std::optional<Catalog> catalog_m;
    mutable std::shared_mutex registry_mutex_m;
    std::unordered_map<std::string, std::shared_ptr<EphemeralTable>> tables_m;
};
