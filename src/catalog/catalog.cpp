#include "catalog/catalog.h"
#include "common/definitions.h"
#include "core/assert.h"
#include "core/row_reader.h"
#include "table/table_factory_interface.h"
#include <stdexcept>

const Schema Catalog::table_catalog_schema = {
    { "table_name", DataType::TEXT },
    { "type", DataType::INTEGER },
    { "num_attributes", DataType::INTEGER },
};

const Schema Catalog::column_catalog_schema = {
    { "attr_name", DataType::TEXT },
    { "rel_name", DataType::TEXT },
    { "type", DataType::INTEGER },
    { "position", DataType::INTEGER },
};

Catalog::Catalog(ITableFactory& factory)
{
    CreateCatalogTables(factory);
    LoadTableSchemas();
    LoadColumnSchemas();
}

void Catalog::CreateCatalogTables(ITableFactory& factory)
{
    bool table_catalog_new = !factory.TableExists(TABLE_CATALOG_TABLE_NAME);
    if (table_catalog_new && !factory.CreateTable(TABLE_CATALOG_TABLE_NAME))
        throw std::runtime_error("Catalog initialization failed: could not create table_catalog table");

    bool column_catalog_new = !factory.TableExists(COLUMN_CATALOG_TABLE_NAME);
    if (column_catalog_new && !factory.CreateTable(COLUMN_CATALOG_TABLE_NAME))
        throw std::runtime_error("Catalog initialization failed: could not create column_catalog table");

    auto table_catalog_storage = factory.GetTable(TABLE_CATALOG_TABLE_NAME);
    if (!table_catalog_storage)
        throw std::runtime_error("Catalog initialization failed: could not get table_catalog table");
    table_catalog_table_m = TableHandle(table_catalog_storage, table_catalog_schema);

    auto column_catalog_storage = factory.GetTable(COLUMN_CATALOG_TABLE_NAME);
    if (!column_catalog_storage)
        throw std::runtime_error("Catalog initialization failed: could not get column_catalog table");
    column_catalog_table_m = TableHandle(column_catalog_storage, column_catalog_schema);

    if (table_catalog_new)
        InitializeTableCatalogTable();
    if (column_catalog_new)
        InitializeColumnCatalogTable();
}

void Catalog::LoadTableSchemas()
{
    for (auto [row_id, row_data] : *table_catalog_table_m) {
        RowReader rr(row_data, table_catalog_schema);
        std::string table_name = rr.Get<DataType::TEXT>(0);
        std::int32_t table_type_int = rr.Get<DataType::INTEGER>(1);
        std::int32_t num_attributes = rr.Get<DataType::INTEGER>(2);

        table_info_m[table_name] = TableInfo {
            .type = static_cast<TableType>(table_type_int),
            .schema = std::vector<RelationAttribute>(static_cast<std::size_t>(num_attributes))
        };
    }
}

void Catalog::LoadColumnSchemas()
{
    for (auto [row_id, row_data] : *column_catalog_table_m) {
        RowReader rr(row_data, column_catalog_schema);
        std::string attribute_name = rr.Get<DataType::TEXT>(0);
        std::string relation_name = rr.Get<DataType::TEXT>(1);
        DataType attribute_type = static_cast<DataType>(rr.Get<DataType::INTEGER>(2));
        std::int32_t position = rr.Get<DataType::INTEGER>(3);

        table_info_m[relation_name].schema[static_cast<std::size_t>(position)].name = attribute_name;
        table_info_m[relation_name].schema[static_cast<std::size_t>(position)].type = attribute_type;
    }
}

bool Catalog::AddTable(std::string_view table_name, TableType table_type, const Schema& schema)
{
    if (table_info_m.contains(std::string(table_name)))
        return false;

    table_catalog_table_m->insert_row({
        Value(std::string(table_name)),
        Value(static_cast<std::int32_t>(table_type)),
        Value(static_cast<std::int32_t>(schema.size())),
    });

    std::int32_t position = 0;
    for (const auto& attribute : schema) {
        column_catalog_table_m->insert_row({
            Value(std::string(attribute.name)),
            Value(std::string(table_name)),
            Value(static_cast<std::int32_t>(attribute.type)),
            Value(position),
        });
        position++;
    }
    table_info_m[std::string(table_name)] = TableInfo {
        .type = table_type,
        .schema = schema
    };
    return true;
}

bool Catalog::RemoveTable(std::string_view table_name)
{
    if (!table_info_m.contains(std::string(table_name)))
        return false;

    // Collect column row IDs to delete, then delete after iteration to avoid
    // invalidating the iterator while traversing.
    {
        std::vector<row_id_t> to_delete;
        for (auto [row_id, row_data] : *column_catalog_table_m) {
            RowReader rr(row_data, column_catalog_schema);
            if (rr.Get<DataType::TEXT>(1) == table_name)
                to_delete.push_back(row_id);
        }
        for (row_id_t rid : to_delete)
            column_catalog_table_m->delete_row(rid);
    }

    // Find and delete the table's entry from table_catalog.
    {
        std::optional<row_id_t> table_rid;
        for (auto [row_id, row_data] : *table_catalog_table_m) {
            RowReader rr(row_data, table_catalog_schema);
            if (rr.Get<DataType::TEXT>(0) == table_name) {
                table_rid = row_id;
                break;
            }
        }
        if (table_rid)
            table_catalog_table_m->delete_row(*table_rid);
    }

    // Remove from in-memory cache
    table_info_m.erase(std::string(table_name));

    return true;
}

bool Catalog::TableExists(std::string_view table_name) const
{
    return table_info_m.contains(std::string(table_name));
}

Schema Catalog::GetSchema(std::string_view table_name) const
{
    std::string table_name_str = std::string(table_name);
    YADB_ASSERT(TableExists(table_name), "Table does not exist");

    return table_info_m.at(table_name_str).schema;
}

TableType Catalog::GetTableType(std::string_view table_name) const
{
    std::string table_name_str = std::string(table_name);
    YADB_ASSERT(table_info_m.contains(table_name_str), "Table does not exist");

    return table_info_m.at(table_name_str).type;
}

void Catalog::InitializeTableCatalogTable()
{
    table_catalog_table_m->insert_row({
        Value(std::string("table_catalog")),
        Value(static_cast<std::int32_t>(TableType::InMemory)),
        Value(static_cast<std::int32_t>(table_catalog_schema.size())),
    });
    table_catalog_table_m->insert_row({
        Value(std::string("column_catalog")),
        Value(static_cast<std::int32_t>(TableType::InMemory)),
        Value(static_cast<std::int32_t>(column_catalog_schema.size())),
    });
}

void Catalog::InitializeColumnCatalogTable()
{
    std::int32_t position = 0;
    for (const auto& attribute : table_catalog_schema) {
        column_catalog_table_m->insert_row({
            Value(std::string(attribute.name)),
            Value(std::string("table_catalog")),
            Value(static_cast<std::int32_t>(attribute.type)),
            Value(position),
        });
        position++;
    }

    position = 0;
    for (const auto& attribute : column_catalog_schema) {
        column_catalog_table_m->insert_row({
            Value(std::string(attribute.name)),
            Value(std::string("column_catalog")),
            Value(static_cast<std::int32_t>(attribute.type)),
            Value(position),
        });
        position++;
    }
}
