#include "storage/in_memory/in_memory_table_iterator.h"

InMemoryTableIterator::InMemoryTableIterator(std::map<row_id_t, std::vector<std::byte>>& data)
    : data_m(data)
    , iterator_m(data.begin())
    , closed_m(false)
{
}

InMemoryTableIterator::~InMemoryTableIterator()
{
    if (!closed_m) {
        close();
    }
}

std::optional<Expected<Row, TableError>> InMemoryTableIterator::next()
{
    if (closed_m) {
        return std::nullopt;
    }

    if (iterator_m == data_m.end()) {
        return std::nullopt;  // End of iteration
    }

    const auto& [row_id, row_data] = *iterator_m;
    std::span<const std::byte> data_span(row_data.data(), row_data.size());
    Row row = std::make_pair(row_id, data_span);

    ++iterator_m;  // Advance to next row
    return Expected<Row, TableError>(row);
}

std::optional<TableError> InMemoryTableIterator::seek(row_id_t rid)
{
    if (closed_m) {
        return TableError::INVALID_ROW_ID;
    }

    iterator_m = data_m.find(rid);
    if (iterator_m == data_m.end()) {
        return TableError::INVALID_ROW_ID;
    }

    return std::nullopt;
}

void InMemoryTableIterator::close()
{
    if (closed_m) {
        return;
    }
    closed_m = true;
}
