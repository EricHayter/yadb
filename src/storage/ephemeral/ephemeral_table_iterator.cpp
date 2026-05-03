#include "storage/ephemeral/ephemeral_table_iterator.h"
#include <stdexcept>

EphemeralTableIterator::EphemeralTableIterator(std::map<row_id_t, std::vector<std::byte>>& data)
    : data_m(data)
    , iterator_m(data.begin())
    , closed_m(false)
{
}

EphemeralTableIterator::~EphemeralTableIterator()
{
    if (!closed_m) {
        close();
    }
}

std::optional<Row> EphemeralTableIterator::next()
{
    if (closed_m) {
        return std::nullopt;
    }

    if (iterator_m == data_m.end()) {
        return std::nullopt; // End of iteration
    }

    const auto& [row_id, row_data] = *iterator_m;
    std::span<const std::byte> data_span(row_data.data(), row_data.size());
    Row row = std::make_pair(row_id, data_span);

    ++iterator_m; // Advance to next row
    return row;
}

void EphemeralTableIterator::seek(row_id_t rid)
{
    if (closed_m) {
        throw std::runtime_error("Cannot seek on closed iterator");
    }

    iterator_m = data_m.find(rid);
    if (iterator_m == data_m.end()) {
        throw std::invalid_argument("Invalid row_id in seek");
    }
}

void EphemeralTableIterator::close()
{
    if (closed_m) {
        return;
    }
    closed_m = true;
}
