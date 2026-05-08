#include "table/table_manager.h"
#include "core/assert.h"
#include "storage/ephemeral/ephemeral_table_factory.h"
#include "storage/on_disk/table/disk_table_factory.h"
#include <stdexcept>

TableManager::TableManager(TableType default_storage_engine)
{
    // Create default factory based on specified storage engine
    std::shared_ptr<ITableFactory> default_factory;
    switch (default_storage_engine) {
    case TableType::InMemory:
        default_factory = std::make_shared<EphemeralTableFactory>();
        break;
    case TableType::Disk:
        default_factory = std::make_shared<DiskTableFactory>();
        break;
    default:
        throw std::runtime_error("Unknown default storage engine type");
    }

    factories_m[default_storage_engine] = default_factory;

    // Initialize catalog using the default factory
    catalog_m = std::make_unique<Catalog>(*default_factory);
    default_factory->SetCatalog(*catalog_m);
}

ITableFactory& TableManager::GetFactory(TableType type)
{
    // If factory exists, return it
    if (factories_m.contains(type)) {
        return *factories_m.at(type);
    }

    // Lazily create factory based on type
    std::shared_ptr<ITableFactory> factory;
    switch (type) {
    case TableType::InMemory:
        factory = std::make_shared<EphemeralTableFactory>();
        break;
    case TableType::Disk:
        factory = std::make_shared<DiskTableFactory>();
        break;
    default:
        throw std::runtime_error("Unknown table type");
    }

    // Set catalog on newly created factory
    factory->SetCatalog(*catalog_m);

    factories_m[type] = factory;
    return *factory;
}

bool TableManager::CreateTable(std::string_view name, TableType type, const Schema& schema)
{
    std::string table_name(name);
    if (catalog_m->TableExists(name))
        return false;

    ITableFactory& factory = GetFactory(type);
    bool created_table = factory.CreateTable(name, schema);

    if (!created_table)
        return false;

    return catalog_m->AddTable(name, type, schema);
}

bool TableManager::DeleteTable(std::string_view name)
{
    if (!catalog_m->TableExists(name))
        return false;

    TableType type = catalog_m->GetTableType(name);

    // Remove from catalog first
    if (!catalog_m->RemoveTable(name))
        return false;

    // Delete actual table through factory
    ITableFactory& factory = GetFactory(type);
    return factory.DeleteTable(name);
}

bool TableManager::TableExists(std::string_view name) const
{
    return catalog_m->TableExists(name);
}

std::shared_ptr<Table> TableManager::GetTable(std::string_view name) const
{
    YADB_ASSERT(TableExists(name), "Table does not exist");

    std::string table_name(name);
    TableType table_type = catalog_m->GetTableType(name);

    ITableFactory& factory = const_cast<TableManager*>(this)->GetFactory(table_type);
    return factory.GetTable(name);
}
