#include "catalog/catalog.h"
#include "common/definitions.h"
#include "table/table_manager.h"
#include "core/row_builder.h"
#include "core/row_reader.h"

Catalog::Catalog(TableManager& table_manager)
    : table_manager_m{table_manager},
      column_catalog_schema_m{
          { "attr_name",      DataType::TEXT },
          { "rel_name",       DataType::TEXT },
          { "type",           DataType::INTEGER },
          { "position",       DataType::INTEGER },
      },
      table_catalog_schema_m{
          { "table_name",     DataType::TEXT },
          { "num_attributes", DataType::INTEGER },
      }
{
    InitializeTableCatalog();
    InitializeColumnCatalog();
    LoadTableSchemas();
    LoadColumnSchemas();
}

void Catalog::InitializeTableCatalog() {
    constexpr std::string_view table_catalog_table_name = "table_catalog";

    if (!table_manager_m.TableExists(table_catalog_table_name)) {
        table_manager_m.CreateTable(table_catalog_table_name, table_catalog_schema_m);
    }

    table_catalog_table_m = table_manager_m.GetTable(table_catalog_table_name);
}

void Catalog::InitializeColumnCatalog() {
    constexpr std::string_view column_catalog_table_name = "column_catalog";

    if (!table_manager_m.TableExists(column_catalog_table_name)) {
        table_manager_m.CreateTable(column_catalog_table_name, column_catalog_schema_m);
    }

    column_catalog_table_m = table_manager_m.GetTable(column_catalog_table_name);
}

void Catalog::LoadTableSchemas() {
    auto iter = table_catalog_table_m->iter();
    auto row = iter->next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, table_catalog_schema_m);
        std::string table_name = rr.Get<DataType::TEXT>(0);
        std::int32_t num_attributes = rr.Get<DataType::INTEGER>(1);
        table_schemas_m[table_name] = std::vector<RelationAttribute>(num_attributes);
        row = iter->next();
    }
    iter->close();
}

void Catalog::LoadColumnSchemas() {
    auto iter = column_catalog_table_m->iter();
    auto row = iter->next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, column_catalog_schema_m);
        std::string attribute_name = rr.Get<DataType::TEXT>(0);
        std::string relation_name = rr.Get<DataType::TEXT>(1);
        DataType attribute_type = static_cast<DataType>(rr.Get<DataType::INTEGER>(2));
        std::int32_t position = rr.Get<DataType::INTEGER>(3);

        table_schemas_m[relation_name][position].name = attribute_name;
        table_schemas_m[relation_name][position].type = attribute_type;

        row = iter->next();
    }
    iter->close();
}

bool Catalog::AddTable(std::string_view table_name, const Schema& schema) {
    if (table_schemas_m.contains(std::string(table_name)))
        return false;

    if (!table_manager_m.CreateTable(table_name, schema))
        return false;

    // Create entry in table catalog using type-safe API
    table_catalog_table_m->insert_row({
        Value(std::string(table_name)),
        Value(static_cast<std::int32_t>(schema.size()))
    });

    // Create entries for column catalog
    std::int32_t position = 0;
    for (const auto& attribute: schema) {
        column_catalog_table_m->insert_row({
            Value(std::string(attribute.name)),
            Value(std::string(table_name)),
            Value(static_cast<std::int32_t>(attribute.type)),
            Value(position)
        });
        position++;
    }
    table_schemas_m[std::string(table_name)] = schema;
    return true;
}

bool Catalog::RemoveTable(std::string_view table_name) {
    if (!table_schemas_m.contains(std::string(table_name)))
        return false;

    // Delete all column entries from column_catalog
    auto column_iter = column_catalog_table_m->iter();
    auto row = column_iter->next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, column_catalog_schema_m);
        std::string relation_name = rr.Get<DataType::TEXT>(1);
        if (relation_name == table_name) {
            column_catalog_table_m->delete_row(row_id);
        }
        row = column_iter->next();
    }
    column_iter->close();

    // Delete table entry from table_catalog
    auto table_iter = table_catalog_table_m->iter();
    row = table_iter->next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, table_catalog_schema_m);
        std::string catalog_table_name = rr.Get<DataType::TEXT>(0);
        if (catalog_table_name == table_name) {
            table_catalog_table_m->delete_row(row_id);
            break;
        }
        row = table_iter->next();
    }
    table_iter->close();

    // Delete the actual table
    table_manager_m.DeleteTable(table_name);

    // Remove from in-memory cache
    table_schemas_m.erase(std::string(table_name));

    return true;
}

std::optional<Schema> Catalog::GetSchema(std::string_view table_name)
{
    std::string table_name_str = std::string(table_name);
    if (!table_schemas_m.contains(table_name_str))
        return std::nullopt;

    return table_schemas_m[table_name_str];
}
