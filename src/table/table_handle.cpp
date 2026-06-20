#include "table/table_handle.h"
#include "core/assert.h"
#include "core/row_builder.h"
#include <format>
#include <stdexcept>

TableHandle::TableHandle(std::shared_ptr<Table> storage, Schema schema)
    : storage_m(std::move(storage))
    , schema_m(std::move(schema))
{
    YADB_ASSERT(storage_m != nullptr, "TableHandle requires a non-null storage");
}

row_id_t TableHandle::insert_row(const std::vector<Value>& values)
{
    if (values.size() != schema_m.size())
        throw std::runtime_error(std::format("Value count mismatch: expected {}, got {}", schema_m.size(), values.size()));

    RowBuilder rb;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto expected_type = schema_m[i].type;
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            constexpr DataType actual_type = enum_for<T>;
            if (actual_type != expected_type)
                throw std::runtime_error(std::format("Type mismatch at column {}: expected {}, got {}", i, ToString(expected_type), ToString(actual_type)));
            rb.Push<actual_type>(v);
        }, values[i]);
    }

    return storage_m->insert_row(rb.Data());
}

row_id_t TableHandle::update_row(const row_id_t& rid, const std::vector<Value>& values)
{
    if (values.size() != schema_m.size())
        throw std::runtime_error(std::format("Value count mismatch: expected {}, got {}", schema_m.size(), values.size()));

    RowBuilder rb;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto expected_type = schema_m[i].type;
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            constexpr DataType actual_type = enum_for<T>;
            if (actual_type != expected_type)
                throw std::runtime_error(std::format("Type mismatch at column {}: expected {}, got {}", i, ToString(expected_type), ToString(actual_type)));
            rb.Push<actual_type>(v);
        }, values[i]);
    }

    return storage_m->update_row(rid, rb.Data());
}

void TableHandle::delete_row(const row_id_t& rid)
{
    storage_m->delete_row(rid);
}

TableCursor TableHandle::begin() const
{
    return storage_m->begin();
}

const Schema& TableHandle::GetSchema() const
{
    return schema_m;
}

TableType TableHandle::GetType() const
{
    return storage_m->GetType();
}
