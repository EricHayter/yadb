#pragma once

#include <span>
#include <utility>
#include <optional>
#include "common/definitions.h"
#include "core/expected.h"


using Row = std::pair<const row_id_t, std::span<const std::byte>>;

class Table {
public:
    enum class OperationError {
        // TODO fill this in more once I find more error cases.
        INVALID_ROW_ID,
    };

    virtual ~Table() = default;

    // initialize the iterator
    virtual std::optional<OperationError> scan_init() = 0;

    // scan the next value in the iterator
    virtual Expected<Row, OperationError> scan_next() = 0;

    // move the iterator cursor to the given row id
    virtual std::optional<OperationError> scan_seek(row_id_t rid) = 0;

    // close/cleanup the iterator
    virtual std::optional<OperationError> scan_end() = 0;


    virtual Expected<row_id_t, OperationError> insert_row(std::span<const std::byte> row) = 0;
    virtual std::optional<OperationError> update_row(Row row) = 0;
    virtual std::optional<OperationError> delete_row(const row_id_t& rid) = 0;
};
