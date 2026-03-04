#pragma once

#include <span>
#include <optional>
#include "common/definitions.h"
#include "core/expected.h"
#include "table/table_iterator.h"
#include <memory>

class Table {
public:
    virtual ~Table() = 0;

    virtual std::unique_ptr<TableIterator> iter() = 0;

    virtual Expected<row_id_t, TableError> insert_row(std::span<const std::byte> row) = 0;
    virtual std::optional<TableError> update_row(Row row) = 0;
    virtual std::optional<TableError> delete_row(const row_id_t& rid) = 0;
};
