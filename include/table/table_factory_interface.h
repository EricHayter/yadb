#pragma once

#include "catalog/catalog.h"
#include "common/definitions.h"
#include <memory>
#include <string_view>

// Interface for table factory implementations.
// Defines the core operations that any table factory must support.
// Factories are responsible for creating, managing, and providing access to table instances.
class ITableFactory {
public:
    virtual ~ITableFactory() = default;

    // Set the catalog for this table factory.
    // The catalog contains metadata about all tables.
    virtual void SetCatalog(Catalog& catalog) = 0;

    // Check if a table exists.
    virtual bool TableExists(std::string_view table_name) const = 0;

    // Create a new table with the given schema.
    virtual bool CreateTable(std::string_view table_name, const Schema& schema) = 0;

    // Get a table by name.
    virtual std::shared_ptr<Table> GetTable(std::string_view table_name) = 0;

    // Delete a table by name.
    virtual bool DeleteTable(std::string_view table_name) = 0;
};