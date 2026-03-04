#pragma once

#include "common/definitions.h"
#include "core/expected.h"
#include <optional>

class TableIterator {
    public:
    virtual ~TableIterator() = 0;
    virtual std::optional<Expected<Row, TableError>> next() = 0;

    // move the iterator cursor to the given row id
    virtual std::optional<TableError> seek(row_id_t rid) = 0;
    virtual void close() = 0;
};
