#include "storage/on_disk/disk_table.h"
#include <stdexcept>

DiskTable::DiskTable(const Schema& schema)
    : Table(schema)
{
}

std::unique_ptr<TableIterator> DiskTable::iter()
{
    throw std::runtime_error("DiskTable::iter not yet implemented");
}

row_id_t DiskTable::insert_row_impl(std::span<const std::byte> row)
{
    throw std::runtime_error("DiskTable::insert_row_impl not yet implemented");
}

void DiskTable::update_row(Row row)
{
    throw std::runtime_error("DiskTable::update_row not yet implemented");
}

void DiskTable::delete_row(const row_id_t& rid)
{
    throw std::runtime_error("DiskTable::delete_row not yet implemented");
}

TableType DiskTable::GetType() const
{
    return TableType::Disk;
}
