#include "storage/in_memory/in_memory_table_manager.h"
#include "storage/in_memory/in_memory_table.h"

bool InMemoryTableManager::CreateTable(std::string_view name, const Schema& schema)
{
    std::string table_name(name);

    // Check if table already exists
    if (tables_m.contains(table_name)) {
        return false;
    }

    // Create new in-memory table with schema
    auto table = std::make_shared<InMemoryTable>(schema);
    tables_m[table_name] = table;

    return true;
}

bool InMemoryTableManager::DeleteTable(std::string_view name)
{
    std::string table_name(name);

    auto it = tables_m.find(table_name);
    if (it == tables_m.end()) {
        return false;
    }

    tables_m.erase(it);

    return true;
}

bool InMemoryTableManager::TableExists(std::string_view name)
{
    return tables_m.contains(std::string(name));
}

std::unique_ptr<Table> InMemoryTableManager::GetTable(std::string_view name)
{
    std::string table_name(name);

    auto it = tables_m.find(table_name);
    if (it == tables_m.end()) {
        return nullptr;
    }

    // Return a new unique_ptr that shares ownership with our shared_ptr
    // This allows the caller to have a unique_ptr while we maintain the table in memory
    return std::unique_ptr<Table>(new InMemoryTable(*it->second));
}
