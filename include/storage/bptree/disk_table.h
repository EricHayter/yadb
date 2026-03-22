#pragma once

#include "table/table.h"

class DiskTable : public Table {
public:
    explicit DiskTable(const Schema& schema);
    ~DiskTable() override = default;

    std::unique_ptr<TableIterator> iter() override;
    void update_row(Row row) override;
    void delete_row(const row_id_t& rid) override;

    TableType GetType() const override;

protected:
    row_id_t insert_row_impl(std::span<const std::byte> row) override;
};
