#pragma once

#include "optimizer/operators/iterator.h"
#include "common/definitions.h"
#include <memory>

/* This is just the default projection iterator it WILL NOT remove duplicates.
 * likely going to create a separate operator for that. */
class ProjectionIterator : Iterator {
    public:
    ProjectionIterator(std::unique_ptr<Iterator> iter, Schema schema, std::vector<std::size_t> selected_fields);
    std::optional<std::vector<std::byte>> next() override;
    void close() override;
    const Schema& GetSchema() const { return output_schema_m; }

    private:
    std::unique_ptr<Iterator> iter_m;
    Schema schema_m;
    std::vector<std::size_t> selected_fields_m;
    Schema output_schema_m;
};
