#include "optimizer/operators/projection_iterator.h"
#include "core/row_reader.h"
#include <optional>
#include <vector>

ProjectionIterator::ProjectionIterator(std::unique_ptr<Iterator> iter, Schema schema, std::vector<std::size_t> selected_fields)
    : iter_m(std::move(iter))
    , schema_m(std::move(schema))
    , selected_fields_m(std::move(selected_fields))
{
    // Build output schema from selected fields
    output_schema_m.reserve(selected_fields_m.size());
    for (std::size_t field_idx : selected_fields_m) {
        output_schema_m.push_back(schema_m[field_idx]);
    }
}

std::optional<std::vector<std::byte>> ProjectionIterator::next()
{
    auto row = iter_m->next();
    if (!row.has_value())
        return std::nullopt;

    RowReader row_reader(*row, schema_m);
    std::vector<std::byte> data;

    // Pre-allocate space for efficiency
    std::size_t total_size = 0;
    for (std::size_t selected_field : selected_fields_m) {
        total_size += row_reader.GetSize(selected_field);
    }
    data.reserve(total_size);

    // Copy selected fields
    for (std::size_t selected_field : selected_fields_m) {
        std::size_t offset = row_reader.GetOffset(selected_field);
        std::size_t size = row_reader.GetSize(selected_field);
        data.insert(data.end(),
            row.value().begin() + offset,
            row.value().begin() + offset + size);
    }

    return data;
}

void ProjectionIterator::close()
{
    iter_m->close();
}
