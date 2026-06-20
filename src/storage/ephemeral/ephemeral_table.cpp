#include "storage/ephemeral/ephemeral_table.h"
#include "storage/ephemeral/ephemeral_table_iterator.h"
#include "storage/on_disk/types.h"
#include <stdexcept>

EphemeralTable::EphemeralTable()
    : next_page_id_m(0)
    , next_slot_id_m(0)
{
}

row_id_t EphemeralTable::GenerateRowId()
{
    page_id_t page_id = next_page_id_m;
    slot_id_t slot_id = next_slot_id_m;

    // Simple auto-increment logic
    next_slot_id_m++;
    if (next_slot_id_m == 0) { // Overflow, move to next page
        next_page_id_m++;
    }

    return MakeRowId(page_id, slot_id);
}

TableCursor EphemeralTable::begin()
{
    return TableCursor { std::make_unique<EphemeralTableIterator>(table_data_m) };
}

row_id_t EphemeralTable::insert_row(std::span<const std::byte> row)
{
    row_id_t rid = GenerateRowId();

    // Copy the row data into our storage
    std::vector<std::byte> row_data(row.begin(), row.end());
    table_data_m.insert({ rid, std::move(row_data) });

    return rid;
}

row_id_t EphemeralTable::update_row(const row_id_t& rid, std::span<const std::byte> data)
{
    delete_row(rid);
    return insert_row(data);
}

void EphemeralTable::delete_row(const row_id_t& rid)
{
    auto it = table_data_m.find(rid);
    if (it == table_data_m.end()) {
        throw std::invalid_argument("Invalid row_id in delete_row");
    }

    table_data_m.erase(it);
}

TableType EphemeralTable::GetType() const
{
    return TableType::InMemory;
}
