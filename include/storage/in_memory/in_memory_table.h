#pragma once

#include "table/table.h"
#include <map>
#include <vector>
#include <span>

class InMemoryTable : public Table {
public:
    InMemoryTable();
    ~InMemoryTable() override = default;

    // Iterator interface
    std::unique_ptr<TableIterator> iter() override;

    // CRUD operations
    Expected<row_id_t, TableError> insert_row(std::span<const std::byte> row) override;
    std::optional<TableError> update_row(Row row) override;
    std::optional<TableError> delete_row(const row_id_t& rid) override;

private:
    // Simple std::map storage (sorted by row_id)
    std::map<row_id_t, std::vector<std::byte>> data_m;

    // Auto-incrementing row ID generator
    uint32_t next_page_id_m = 0;
    uint16_t next_slot_id_m = 0;

    // Helper to generate next row_id
    row_id_t GenerateRowId();
};
