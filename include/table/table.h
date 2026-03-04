#pragma once

#include <span>
#include "common/definitions.h"
#include "table/table_iterator.h"
#include <memory>

class Table {
public:
    virtual ~Table() = 0;

    virtual std::unique_ptr<TableIterator> iter() = 0;

    // CRUD operations - throw on error
    virtual row_id_t insert_row(std::span<const std::byte> row) = 0;
    virtual void update_row(Row row) = 0;
    virtual void delete_row(const row_id_t& rid) = 0;
};
