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
    constexpr std::string_view column_catalog_table_name = "column_catalog";

    if (!table_manager_m.TableExists(column_catalog_table_name)) {
        table_manager_m.CreateTable(*this, column_catalog_table_name, column_catalog_schema_m);
    }

    constexpr std::string_view table_catalog_table_name = "table_catalog";

    if (!table_manager_m.TableExists(table_catalog_table_name)) {
        table_manager_m.CreateTable(*this, table_catalog_table_name, table_catalog_schema_m);
    }

    // Intialize each of the schemas based on the table catalog
    table_catalog_table_m = table_manager_m.GetTable(table_catalog_table_name);
    table_catalog_table_m->scan_init();
    auto row = table_catalog_table_m->scan_next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, table_catalog_schema_m);
        std::string table_name = rr.Get<DataType::TEXT>(0);
        std::int32_t num_attributes = rr.Get<DataType::INTEGER>(1);
        table_schemas_m[table_name] = std::vector<RelationAttribute>(num_attributes);
        row = table_catalog_table_m->scan_next();
    }
    table_catalog_table_m->scan_end();

    // Populate the schemas with the information from the column catalog schema
    column_catalog_table_m = table_manager_m.GetTable(column_catalog_table_name);
    column_catalog_table_m->scan_init();
    row = column_catalog_table_m->scan_next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, column_catalog_schema_m);
        std::string attribute_name = rr.Get<DataType::TEXT>(0);
        std::string relation_name = rr.Get<DataType::TEXT>(1);
        DataType attribute_type = static_cast<DataType>(rr.Get<DataType::INTEGER>(2));
        std::int32_t position = rr.Get<DataType::INTEGER>(3);

        table_schemas_m[relation_name][position].name = attribute_name;
        table_schemas_m[relation_name][position].type = attribute_type;

        row = column_catalog_table_m->scan_next();
    }
    column_catalog_table_m->scan_end();
}

bool Catalog::AddTable(std::string_view table_name, const Schema& schema) {
    if (table_schemas_m.contains(std::string(table_name)))
        return false;

    // Create entry in table catalog
    RowBuilder bb;
    bb.Push<DataType::TEXT>(table_name);
    bb.Push<DataType::INTEGER>(static_cast<std::int32_t>(schema.size()));
    table_catalog_table_m->insert_row(bb.Data());

    // Create entries for column catalog
    std::int32_t position = 0;
    for (const auto& attribute: schema) {
        bb = RowBuilder();
        bb.Push<DataType::TEXT>(attribute.name);
        bb.Push<DataType::TEXT>(table_name);
        bb.Push<DataType::INTEGER>((std::int32_t) attribute.type);
        bb.Push<DataType::INTEGER>(position);
        column_catalog_table_m->insert_row(bb.Data());
        position++;
    }
    table_schemas_m[std::string(table_name)] = schema;
    return true;
}

bool Catalog::RemoveTable(std::string_view table_name) {
    if (!table_schemas_m.contains(std::string(table_name)))
        return false;

    // Delete all column entries from column_catalog
    column_catalog_table_m->scan_init();
    auto row = column_catalog_table_m->scan_next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, column_catalog_schema_m);
        std::string relation_name = rr.Get<DataType::TEXT>(1);
        if (relation_name == table_name) {
            column_catalog_table_m->delete_row(row_id);
        }
        row = column_catalog_table_m->scan_next();
    }
    column_catalog_table_m->scan_end();

    // Delete table entry from table_catalog
    table_catalog_table_m->scan_init();
    row = table_catalog_table_m->scan_next();
    while (row) {
        auto &[row_id, row_data] = row.value();
        RowReader rr(row_data, table_catalog_schema_m);
        std::string catalog_table_name = rr.Get<DataType::TEXT>(0);
        if (catalog_table_name == table_name) {
            table_catalog_table_m->delete_row(row_id);
            break;
        }
        row = table_catalog_table_m->scan_next();
    }
    table_catalog_table_m->scan_end();

    // Delete the actual table
    table_manager_m.DeleteTable(*this, table_name);

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
