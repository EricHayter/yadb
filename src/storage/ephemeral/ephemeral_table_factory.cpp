#include "storage/ephemeral/ephemeral_table_factory.h"
#include <mutex>

bool EphemeralTableFactory::TableExists(std::string_view table_name) const
{
    std::shared_lock<std::shared_mutex> lock(registry_mutex_m);
    return tables_m.find(std::string(table_name)) != tables_m.end();
}

bool EphemeralTableFactory::CreateTable(std::string_view table_name, const Schema& schema)
{
    {
        std::shared_lock<std::shared_mutex> lock(registry_mutex_m);
        if (tables_m.find(std::string(table_name)) != tables_m.end()) {
            return false;
        }
    }

    std::unique_lock<std::shared_mutex> lock(registry_mutex_m);
    tables_m[std::string(table_name)] = std::shared_ptr<EphemeralTable>(new EphemeralTable(schema));

    return true;
}

std::shared_ptr<Table> EphemeralTableFactory::GetTable(std::string_view table_name)
{
    std::shared_lock<std::shared_mutex> lock(registry_mutex_m);
    auto it = tables_m.find(std::string(table_name));
    if (it == tables_m.end()) {
        return nullptr;
    }
    return it->second;
}

bool EphemeralTableFactory::DeleteTable(std::string_view table_name)
{
    std::unique_lock<std::shared_mutex> lock(registry_mutex_m);
    auto it = tables_m.find(std::string(table_name));
    if (it == tables_m.end()) {
        return false;
    }
    tables_m.erase(it);

    return true;
}
