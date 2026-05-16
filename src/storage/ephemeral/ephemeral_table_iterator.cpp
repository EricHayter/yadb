#include "storage/ephemeral/ephemeral_table_iterator.h"
#include <stdexcept>

EphemeralTableIterator::EphemeralTableIterator(std::map<row_id_t, std::vector<std::byte>>& data)
    : data_m(data)
    , iterator_m(data.begin())
{
    load_current();
}

TableIterator& EphemeralTableIterator::operator++()
{
    ++iterator_m;
    load_current();
    return *this;
}

void EphemeralTableIterator::seek(row_id_t rid)
{
    iterator_m = data_m.find(rid);
    if (iterator_m == data_m.end())
        throw std::invalid_argument("Invalid row_id in seek");
    load_current();
}

void EphemeralTableIterator::load_current()
{
    if (iterator_m == data_m.end()) {
        current_m = std::nullopt;
        return;
    }
    const auto& [row_id, row_data] = *iterator_m;
    current_m = Row(row_id, std::span<const std::byte>(row_data.data(), row_data.size()));
}
