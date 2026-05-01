#include "storage/in_memory/in_memory_table.h"
#include "storage/in_memory/in_memory_table_iterator.h"
#include "storage/on_disk/types.h"
#include <stdexcept>

// Define the static registry
std::unordered_map<std::string, std::shared_ptr<InMemoryTable>> InMemoryTable::tables_m;

bool InMemoryTable::CreateTable(std::string_view table_name, const Schema& schema) {
    if (tables_m.contains(std::string(table_name)))
        return false;
    tables_m[std::string(table_name)] = std::shared_ptr<InMemoryTable>(new InMemoryTable(schema));
    return true;
}

std::shared_ptr<InMemoryTable> InMemoryTable::GetTable(std::string_view table_name) {
    if (!tables_m.contains(std::string(table_name)))
        return nullptr;
    return tables_m[std::string(table_name)];
}

InMemoryTable::InMemoryTable(const Schema& schema)
    : Table(schema)
    , next_page_id_m(0)
    , next_slot_id_m(0)
{
}

row_id_t InMemoryTable::GenerateRowId()
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

std::unique_ptr<TableIterator> InMemoryTable::iter()
{
    return std::make_unique<InMemoryTableIterator>(table_data_m);
}

row_id_t InMemoryTable::insert_row_impl(std::span<const std::byte> row)
{
    row_id_t rid = GenerateRowId();

    // Copy the row data into our storage
    std::vector<std::byte> row_data(row.begin(), row.end());
    table_data_m.insert({ rid, std::move(row_data) });

    return rid;
}

row_id_t InMemoryTable::update_row(const row_id_t& rid, std::span<const std::byte> data)
{
    delete_row(rid);
    return insert_row_impl(data);
}

void InMemoryTable::delete_row(const row_id_t& rid)
{
    auto it = table_data_m.find(rid);
    if (it == table_data_m.end()) {
        throw std::invalid_argument("Invalid row_id in delete_row");
    }

    table_data_m.erase(it);
}

TableType InMemoryTable::GetType() const
{
    return TableType::InMemory;
}
