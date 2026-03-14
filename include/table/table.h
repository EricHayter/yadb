#pragma once

#include <span>
#include <vector>
#include "common/definitions.h"
#include "table/table_iterator.h"
#include <memory>

class Table {
public:
    explicit Table(const Schema& schema);
    virtual ~Table() = 0;

    virtual std::unique_ptr<TableIterator> iter() = 0;

    // Public type-safe CRUD operations
    row_id_t insert_row(const std::vector<Value>& values);
    virtual void update_row(Row row) = 0;
    virtual void delete_row(const row_id_t& rid) = 0;

    // Schema accessor
    const Schema& GetSchema() const;

protected:
    // Protected implementation interface for subclasses
    virtual row_id_t insert_row_impl(std::span<const std::byte> row) = 0;

    // Schema stored by the table
    Schema schema_m;
};
