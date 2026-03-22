#include "table/table_manager.h"
#include "storage/in_memory/in_memory_table.h"
#include "storage/bptree/disk_table.h"
#include <stdexcept>

bool TableManager::CreateTable(std::string_view name, TableType type, const Schema& schema)
{
    std::string table_name(name);

    // Check if table already exists
    if (tables_m.contains(table_name)) {
        return false;
    }

    // Create the appropriate table type
    std::shared_ptr<Table> table;
    switch (type) {
        case TableType::InMemory:
            table = std::make_shared<InMemoryTable>(schema);
            break;
        case TableType::Disk:
            table = std::make_shared<DiskTable>(schema);
            break;
        default:
            throw std::runtime_error("Unknown table type");
    }

    tables_m[table_name] = table;
    return true;
}

bool TableManager::DeleteTable(std::string_view name)
{
    std::string table_name(name);

    if (!tables_m.contains(table_name)) {
        return false;
    }

    tables_m.erase(table_name);
    return true;
}

bool TableManager::TableExists(std::string_view name) const
{
    return tables_m.contains(std::string(name));
}

std::shared_ptr<Table> TableManager::GetTable(std::string_view name) const
{
    std::string table_name(name);

    auto it = tables_m.find(table_name);
    if (it == tables_m.end()) {
        return nullptr;
    }

    return it->second;
}
