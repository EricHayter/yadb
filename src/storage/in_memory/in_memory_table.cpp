#include "storage/in_memory/in_memory_table.h"
#include "core/expected.h"
#include <algorithm>

InMemoryTable::InMemoryTable()
    : scan_active_m(false)
    , next_page_id_m(0)
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

std::optional<Table::OperationError> InMemoryTable::scan_init()
{
    scan_iterator_m = data_m.begin();
    scan_active_m = true;
    return std::nullopt;
}

Expected<Row, Table::OperationError> InMemoryTable::scan_next()
{
    if (!scan_active_m) {
        return make_unexpected(OperationError::INVALID_ROW_ID);
    }

    if (scan_iterator_m == data_m.end()) {
        return make_unexpected(OperationError::INVALID_ROW_ID);
    }

    const auto& [row_id, row_data] = *scan_iterator_m;
    std::span<const std::byte> data_span(row_data.data(), row_data.size());
    Row row = std::make_pair(row_id, data_span);

    ++scan_iterator_m;
    return row;
}

std::optional<Table::OperationError> InMemoryTable::scan_seek(row_id_t rid)
{
    if (!scan_active_m) {
        return OperationError::INVALID_ROW_ID;
    }

    scan_iterator_m = data_m.find(rid);
    if (scan_iterator_m == data_m.end()) {
        return OperationError::INVALID_ROW_ID;
    }

    return std::nullopt;
}

std::optional<Table::OperationError> InMemoryTable::scan_end()
{
    scan_active_m = false;
    return std::nullopt;
}

Expected<row_id_t, Table::OperationError> InMemoryTable::insert_row(std::span<const std::byte> row)
{
    row_id_t rid = GenerateRowId();

    // Copy the row data into our storage
    std::vector<std::byte> row_data(row.begin(), row.end());
    data_m.insert({rid, std::move(row_data)});

    return rid;
}

std::optional<Table::OperationError> InMemoryTable::update_row(Row row)
{
    const auto& [row_id, row_data] = row;

    auto it = data_m.find(row_id);
    if (it == data_m.end()) {
        return OperationError::INVALID_ROW_ID;
    }

    // Update the row data
    it->second.assign(row_data.begin(), row_data.end());

    return std::nullopt;
}

std::optional<Table::OperationError> InMemoryTable::delete_row(const row_id_t& rid)
{
    auto it = data_m.find(rid);
    if (it == data_m.end()) {
        return OperationError::INVALID_ROW_ID;
    }

    data_m.erase(it);
    return std::nullopt;
}
