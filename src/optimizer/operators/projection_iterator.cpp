#include "optimizer/operators/projection_iterator.h"
#include "core/row_reader.h"

ProjectionIterator::ProjectionIterator(std::unique_ptr<Iterator> iter, Schema schema, std::vector<std::size_t> selected_fields)
    : iter_m(std::move(iter))
    , schema_m(std::move(schema))
    , selected_fields_m(std::move(selected_fields))
{
    output_schema_m.reserve(selected_fields_m.size());
    for (std::size_t field_idx : selected_fields_m) {
        output_schema_m.push_back(schema_m[field_idx]);
    }
    load_current();
}

Iterator& ProjectionIterator::operator++()
{
    ++(*iter_m);
    load_current();
    return *this;
}

void ProjectionIterator::load_current()
{
    if (*iter_m == std::default_sentinel_t {}) {
        current_m = std::nullopt;
        return;
    }
    current_m = project(**iter_m);
}

std::vector<std::byte> ProjectionIterator::project(const std::vector<std::byte>& row) const
{
    RowReader row_reader(row, schema_m);
    std::vector<std::byte> data;

    std::size_t total_size = 0;
    for (std::size_t field : selected_fields_m) {
        total_size += row_reader.GetSize(field);
    }
    data.reserve(total_size);

    for (std::size_t field : selected_fields_m) {
        std::size_t offset = row_reader.GetOffset(field);
        std::size_t size = row_reader.GetSize(field);
        data.insert(data.end(),
            row.begin() + static_cast<std::ptrdiff_t>(offset),
            row.begin() + static_cast<std::ptrdiff_t>(offset + size));
    }

    return data;
}
