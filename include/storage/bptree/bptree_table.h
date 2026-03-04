#pragma once

#include "table/table.h"

class BPTreeTable : public Table {
public:
    virtual ~BPTreeTable() override = default;

    std::unique_ptr<TableIterator> iter() override;
    Expected<row_id_t, TableError> insert_row(std::span<const std::byte> row) override;
    std::optional<TableError> update_row(Row row) override;
    std::optional<TableError> delete_row(const row_id_t& rid) override;
};
