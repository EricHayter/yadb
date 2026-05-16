#pragma once

#include "common/definitions.h"
#include "optimizer/operators/iterator.h"
#include <memory>

class ProjectionIterator : public Iterator {
public:
    ProjectionIterator(std::unique_ptr<Iterator> iter, Schema schema, std::vector<std::size_t> selected_fields);

    Iterator& operator++() override;
    const Schema& GetSchema() const { return output_schema_m; }

private:
    void load_current();
    std::vector<std::byte> project(const std::vector<std::byte>& row) const;

    std::unique_ptr<Iterator> iter_m;
    Schema schema_m;
    std::vector<std::size_t> selected_fields_m;
    Schema output_schema_m;
};
