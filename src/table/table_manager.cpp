#include "table/table_manager.h"
#include "core/assert.h"
#include "storage/on_disk/table/disk_table.h"
#include <stdexcept>

TableManager::TableManager()
    : ephemeral_factory_m(std::make_unique<EphemeralTableFactory>())
    , catalog_m(*ephemeral_factory_m)
{
    // Set the catalog on the factory after it's created
    ephemeral_factory_m->SetCatalog(catalog_m);
}

// Explicit template instantiation for common cases
template bool TableManager::CreateTable<>(std::string_view name, TableType type, const Schema& schema);

template<typename... Args>
bool TableManager::CreateTable(std::string_view name, TableType type, const Schema& schema, const Args&... args)
{
    std::string table_name(name);
    if (catalog_m.TableExists(name))
        return false;

    bool created_table = false;
    switch (type) {
    case TableType::InMemory:
        created_table = ephemeral_factory_m->CreateTable(name, schema);
        break;
    case TableType::Disk:
        throw std::runtime_error("DiskTable::CreateTable not implemented yet");
    default:
        throw std::runtime_error("Unknown table type");
    }

    if (!created_table)
        return false;

    return catalog_m.AddTable(name, type, schema);
}

bool TableManager::DeleteTable(std::string_view name)
{
    if (!catalog_m.TableExists(name))
        return false;

    TableType type = catalog_m.GetTableType(name);

    // Remove from catalog first
    if (!catalog_m.RemoveTable(name))
        return false;

    // Delete actual table based on type
    switch (type) {
    case TableType::InMemory:
        if (!ephemeral_factory_m->DeleteTable(name)) {
            return false;
        }
        break;
    case TableType::Disk:
        throw std::runtime_error("DiskTable::DeleteTable not implemented yet");
    default:
        throw std::runtime_error("Unknown table type");
    }

    return true;
}

bool TableManager::TableExists(std::string_view name) const
{
    return catalog_m.TableExists(name);
}

// Explicit template instantiation for common cases
template std::shared_ptr<Table> TableManager::GetTable<>(std::string_view name) const;

template<typename... Args>
std::shared_ptr<Table> TableManager::GetTable(std::string_view name, const Args&... args) const
{
    YADB_ASSERT(TableExists(name), "Table does not exist");

    std::string table_name(name);
    TableType table_type = catalog_m.GetTableType(name);

    switch (table_type) {
    case TableType::InMemory:
        return ephemeral_factory_m->GetTable(name);
    case TableType::Disk:
        throw std::runtime_error("DiskTable::CreateTable not implemented yet");
    default:
        throw std::runtime_error("Unknown table type");
    }
}
