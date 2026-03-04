#pragma once

#include "optimizer/operators/iterator.h"
#include "common/definitions.h"

/* This is just the default projection iterator it WILL NOT remove duplicates.
 * likely going to create a separate operator for that. */
class ProjectionIterator : Iterator {
    public:
    ProjectionIterator(Iterator& iter, const Schema& schema, const std::vector<std::size_t>& selected_fields);
    std::optional<std::vector<std::byte>> next() override;
    void close() override;
    const Schema& GetSchema() const { return output_schema_m; }

    private:
    Iterator& iter_m;
    const Schema& schema_m;
    const std::vector<std::size_t>& selected_fields_m;
    Schema output_schema_m;
};
