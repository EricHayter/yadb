#include "storage/bptree/bptree_table.h"
#include <stdexcept>

BPTreeTable::BPTreeTable(const Schema& schema)
    : Table(schema)
{
}

std::unique_ptr<TableIterator> BPTreeTable::iter()
{
    throw std::runtime_error("BPTreeTable::iter not yet implemented");
}

row_id_t BPTreeTable::insert_row_impl(std::span<const std::byte> row)
{
    throw std::runtime_error("BPTreeTable::insert_row_impl not yet implemented");
}

void BPTreeTable::update_row(Row row)
{
    throw std::runtime_error("BPTreeTable::update_row not yet implemented");
}

void BPTreeTable::delete_row(const row_id_t& rid)
{
    throw std::runtime_error("BPTreeTable::delete_row not yet implemented");
}
