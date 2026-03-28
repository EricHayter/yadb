#include "table/table_manager.h"
#include "core/assert.h"
#include "storage/in_memory/in_memory_table.h"
#include "storage/on_disk/disk_table.h"
#include <stdexcept>

TableManager::TableManager()
    : catalog_m([]() {
        // Bootstrap catalog tables
        constexpr std::string_view table_catalog_name = "table_catalog";
        constexpr std::string_view column_catalog_name = "column_catalog";

        // Create catalog tables if they don't exist
        if (!InMemoryTable::GetTable(table_catalog_name)) {
            InMemoryTable::CreateTable(table_catalog_name, Catalog::table_catalog_schema);
        }
        if (!InMemoryTable::GetTable(column_catalog_name)) {
            InMemoryTable::CreateTable(column_catalog_name, Catalog::column_catalog_schema);
        }

        // Get handles
        auto table_catalog = InMemoryTable::GetTable(table_catalog_name);
        auto column_catalog = InMemoryTable::GetTable(column_catalog_name);

        return Catalog(table_catalog, column_catalog);
    }())
{
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
        created_table = InMemoryTable::CreateTable(name, schema);
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
        // InMemoryTable doesn't have a DeleteTable method yet
        // The static registry keeps the table alive
        // TODO: Add static DeleteTable method to InMemoryTable
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
        return InMemoryTable::GetTable(name);
    case TableType::Disk:
        throw std::runtime_error("DiskTable::CreateTable not implemented yet");
    default:
        throw std::runtime_error("Unknown table type");
    }
}
