#include "storage/in_memory/in_memory_table.h"
#include "storage/in_memory/in_memory_table_iterator.h"
#include <stdexcept>

InMemoryTable::InMemoryTable(const Schema& schema)
    : Table(schema)
    , next_page_id_m(0)
    , next_slot_id_m(0)
{
}

InMemoryTable::InMemoryTable(const InMemoryTable& other)
    : Table(other.schema_m)
    , data_m(other.data_m)
    , next_page_id_m(other.next_page_id_m)
    , next_slot_id_m(other.next_slot_id_m)
{
}

row_id_t InMemoryTable::GenerateRowId()
{
    row_id_t rid { next_page_id_m, next_slot_id_m };

    // Simple auto-increment logic
    next_slot_id_m++;
    if (next_slot_id_m == 0) { // Overflow, move to next page
        next_page_id_m++;
    }

    return rid;
}

std::unique_ptr<TableIterator> InMemoryTable::iter()
{
    return std::make_unique<InMemoryTableIterator>(data_m);
}

row_id_t InMemoryTable::insert_row_impl(std::span<const std::byte> row)
{
    row_id_t rid = GenerateRowId();

    // Copy the row data into our storage
    std::vector<std::byte> row_data(row.begin(), row.end());
    data_m.insert({rid, std::move(row_data)});

    return rid;
}

void InMemoryTable::update_row(Row row)
{
    const auto& [row_id, row_data] = row;

    auto it = data_m.find(row_id);
    if (it == data_m.end()) {
        throw std::invalid_argument("Invalid row_id in update_row");
    }

    // Update the row data
    it->second.assign(row_data.begin(), row_data.end());
}

void InMemoryTable::delete_row(const row_id_t& rid)
{
    auto it = data_m.find(rid);
    if (it == data_m.end()) {
        throw std::invalid_argument("Invalid row_id in delete_row");
    }

    data_m.erase(it);
}

TableType InMemoryTable::GetType() const
{
    return TableType::InMemory;
}
