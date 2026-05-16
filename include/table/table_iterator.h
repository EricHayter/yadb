#pragma once

#include "common/definitions.h"
#include "core/assert.h"
#include <iterator>
#include <optional>

class TableIterator {
public:
    using value_type = Row;

    // Thin non-owning handle returned by begin() so range-for can store it by value.
    struct Ref {
        TableIterator* it;
        const value_type& operator*() const { return **it; }
        Ref& operator++() { ++(*it); return *this; }
        bool operator==(std::default_sentinel_t) const { return *it == std::default_sentinel_t {}; }
    };

    TableIterator() = default;
    virtual ~TableIterator() = default;
    TableIterator(const TableIterator&) = delete;
    TableIterator& operator=(const TableIterator&) = delete;

    const value_type& operator*() const
    {
        YADB_ASSERT(current_m.has_value(), "Dereferencing exhausted iterator");
        return *current_m;
    }

    virtual TableIterator& operator++() = 0;

    bool operator==(std::default_sentinel_t) const { return !current_m.has_value(); }

    Ref begin() { return Ref { this }; }
    std::default_sentinel_t end() const { return {}; }

    virtual void seek(row_id_t rid) = 0;

protected:
    std::optional<value_type> current_m;
};
