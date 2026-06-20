#pragma once

#include "common/definitions.h"
#include <iterator>
#include <memory>

class TableIterator {
public:
    using value_type = Row;
    using Ptr = std::unique_ptr<TableIterator>;

    TableIterator() = default;
    virtual ~TableIterator() = default;
    TableIterator(const TableIterator&) = delete;
    TableIterator& operator=(const TableIterator&) = delete;

    virtual value_type operator*() const = 0;
    virtual TableIterator& operator++() = 0;
    virtual bool operator==(std::default_sentinel_t) const = 0;

    virtual void seek(row_id_t rid) = 0;
};
