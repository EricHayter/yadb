#pragma once

#include "common/definitions.h"
#include "table/table_iterator.h"
#include <iterator>

// Value-semantic adaptor over a polymorphic TableIterator. This is the public
// iterator type for table scans: it owns the underlying iterator and forwards
// the iteration protocol through it, so tables can be used with range-for and
// standard algorithms while TableIterator stays an implementation detail.
//
// Move-only (the owned iterator is move-only). The Row returned by operator* is
// valid until the next operator++ (see TableIterator).
//
// Why this exists: a Table handle is iterated generically without knowing its
// backend (in-memory vs on-disk), and the backend is chosen at runtime, so the
// dispatch is unavoidable. TableIterator is the erasure boundary (the vtable);
// this cursor is just the thin value-semantic lid that makes it usable with
// range-for. The alternative mechanism would be a closed std::variant of
// concrete iterators visited per row; the vtable is preferred here because it
// keeps the backend set open. Templating consumers on the table type would
// remove the dispatch but force every table-handling function to be a template,
// which isn't worth it.
class TableCursor {
public:
    using value_type = Row;

    // Adopts the erased iterator; callers build it with make_unique.
    explicit TableCursor(TableIterator::Ptr it)
        : it_m(std::move(it))
    {
    }

    Row operator*() const { return **it_m; }

    TableCursor& operator++()
    {
        ++*it_m;
        return *this;
    }

    bool operator==(std::default_sentinel_t sentinel) const { return *it_m == sentinel; }

private:
    TableIterator::Ptr it_m;
};
