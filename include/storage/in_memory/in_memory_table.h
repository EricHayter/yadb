#pragma once

#include "table/table.h"
#include <map>
#include <unordered_map>
#include <span>
#include <vector>

class InMemoryTable : public Table {
public:
    static bool CreateTable(std::string_view table_name, const Schema& schema);
    static std::shared_ptr<InMemoryTable> GetTable(std::string_view table_name);

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
    using TableData = std::map<row_id_t, std::vector<std::byte>>;

    InMemoryTable(const Schema& schema);

    static std::unordered_map<std::string, std::shared_ptr<InMemoryTable>> tables_m;

    TableData table_data_m;

    // Auto-incrementing row ID generator
    uint32_t next_page_id_m = 0;
    uint16_t next_slot_id_m = 0;

    // Helper to generate next row_id
    row_id_t GenerateRowId();
};
