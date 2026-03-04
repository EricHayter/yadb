#pragma once

#include "common/definitions.h"
#include <optional>

class TableIterator {
public:
    virtual ~TableIterator() = 0;

    // Returns the next row, or std::nullopt if end of iteration
    // Throws on error (e.g., I/O failure, corruption)
    virtual std::optional<Row> next() = 0;

    // Move the iterator cursor to the given row id
    // Throws if row_id is invalid or iterator is closed
    virtual void seek(row_id_t rid) = 0;

    virtual void close() = 0;
};
