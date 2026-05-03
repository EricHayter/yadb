#pragma once

#include "catalog/catalog.h"
#include "table/table_factory_interface.h"
#include "storage/ephemeral/ephemeral_table.h"
#include <memory>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>

class EphemeralTableFactory : public ITableFactory {
public:
    // Default constructor - use SetCatalog() to attach a catalog later.
    // Use this when the catalog is not available at factory construction time.
    EphemeralTableFactory() = default;

    // Constructor with catalog - attaches the catalog immediately.
    // Use this when the catalog is available at factory construction time.
    explicit EphemeralTableFactory(const Catalog& catalog) { SetCatalog(catalog); }

    bool TableExists(std::string_view table_name) const override;
    bool CreateTable(std::string_view table_name, const Schema& schema) override;
    std::shared_ptr<Table> GetTable(std::string_view table_name) override;
    bool DeleteTable(std::string_view table_name) override;

private:
    mutable std::shared_mutex registry_mutex_m;
    std::unordered_map<std::string, std::shared_ptr<EphemeralTable>> tables_m;
};
