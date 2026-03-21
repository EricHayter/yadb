#pragma once

#include "table/table.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <span>

class InMemoryTable : public Table {
public:
    explicit InMemoryTable(const Schema& schema);
    InMemoryTable(const InMemoryTable& other);
    ~InMemoryTable() override = default;

    // Iterator interface
    std::unique_ptr<TableIterator> iter() override;

    // CRUD operations
    void update_row(Row row) override;
    void delete_row(const row_id_t& rid) override;

    // Table type
    TableType GetType() const override;

protected:
    // Protected implementation interface
    row_id_t insert_row_impl(std::span<const std::byte> row) override;

private:
    // Simple std::map storage (sorted by row_id)
    std::map<row_id_t, std::vector<std::byte>> data_m;

    // Auto-incrementing row ID generator
    uint32_t next_page_id_m = 0;
    uint16_t next_slot_id_m = 0;

    // Helper to generate next row_id
    row_id_t GenerateRowId();
};
