#pragma once

#include "common/definitions.h"
#include "table/table.h"
#include <memory>
#include <vector>

// Schema-aware handle over a Table storage instance.
// Provides value-level insert with type validation and serialization.
// Use this at call sites that work with SQL values; use Table directly
// only when you already have serialized bytes (e.g. catalog internals).
class TableHandle {
public:
    TableHandle(std::shared_ptr<Table> storage, Schema schema);

    row_id_t insert_row(const std::vector<Value>& values);
    row_id_t update_row(const row_id_t& rid, const std::vector<Value>& values);
    void delete_row(const row_id_t& rid);

    TableCursor begin() const;
    std::default_sentinel_t end() const { return std::default_sentinel; }

    const Schema& GetSchema() const;
    TableType GetType() const;

private:
    std::shared_ptr<Table> storage_m;
    Schema schema_m;
};
