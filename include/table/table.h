#pragma once

#include "common/definitions.h"
#include "table/table_iterator.h"
#include <memory>
#include <span>

class Table {
public:
    virtual ~Table() = 0;

    virtual std::unique_ptr<TableIterator> iter() = 0;

    virtual row_id_t insert_row(std::span<const std::byte> row) = 0;
    virtual row_id_t update_row(const row_id_t& rid, std::span<const std::byte> data) = 0;
    virtual void delete_row(const row_id_t& rid) = 0;

    virtual TableType GetType() const = 0;
};
