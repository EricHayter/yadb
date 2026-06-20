#pragma once

#include "common/definitions.h"
#include "table/table_cursor.h"
#include <span>

class Table {
public:
    using Ptr = std::shared_ptr<Table>;
    virtual ~Table() = 0;

    virtual TableCursor begin() = 0;
    std::default_sentinel_t end() { return std::default_sentinel; }

    virtual row_id_t insert_row(std::span<const std::byte> row) = 0;
    virtual row_id_t update_row(const row_id_t& rid, std::span<const std::byte> data) = 0;
    virtual void delete_row(const row_id_t& rid) = 0;

    virtual TableType GetType() const = 0;
};
