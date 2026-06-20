#pragma once

#include "table/table.h"
#include <map>
#include <span>
#include <vector>

class EphemeralTableFactory;

class EphemeralTable : public Table {
public:
    ~EphemeralTable() override = default;

    // Iterator interface
    TableCursor begin() override;

    // CRUD operations
    row_id_t insert_row(std::span<const std::byte> row) override;
    row_id_t update_row(const row_id_t& rid, std::span<const std::byte> data) override;
    void delete_row(const row_id_t& rid) override;

    // Table type
    TableType GetType() const override;

private:
    // Simple std::map storage (sorted by row_id)
    using TableData = std::map<row_id_t, std::vector<std::byte>>;

    EphemeralTable();

    friend class EphemeralTableFactory;

    TableData table_data_m;

    // Auto-incrementing row ID generator
    uint32_t next_page_id_m = 0;
    uint16_t next_slot_id_m = 0;

    // Helper to generate next row_id
    row_id_t GenerateRowId();
};
