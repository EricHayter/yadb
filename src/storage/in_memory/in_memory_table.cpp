#include "storage/in_memory/in_memory_table.h"
#include "storage/in_memory/in_memory_table_iterator.h"
#include "core/expected.h"
#include <algorithm>

InMemoryTable::InMemoryTable()
    : next_page_id_m(0)
    , next_slot_id_m(0)
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

Expected<row_id_t, TableError> InMemoryTable::insert_row(std::span<const std::byte> row)
{
    row_id_t rid = GenerateRowId();

    // Copy the row data into our storage
    std::vector<std::byte> row_data(row.begin(), row.end());
    data_m.insert({rid, std::move(row_data)});

    return rid;
}

std::optional<TableError> InMemoryTable::update_row(Row row)
{
    const auto& [row_id, row_data] = row;

    auto it = data_m.find(row_id);
    if (it == data_m.end()) {
        return TableError::INVALID_ROW_ID;
    }

    // Update the row data
    it->second.assign(row_data.begin(), row_data.end());

    return std::nullopt;
}

std::optional<TableError> InMemoryTable::delete_row(const row_id_t& rid)
{
    auto it = data_m.find(rid);
    if (it == data_m.end()) {
        return TableError::INVALID_ROW_ID;
    }

    data_m.erase(it);
    return std::nullopt;
}
