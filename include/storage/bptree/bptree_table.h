#pragma once

#include "table/table.h"

class BPTreeTable : public Table {
public:
    virtual ~BPTreeTable() override = default;

    std::unique_ptr<TableIterator> iter() override;
    row_id_t insert_row(std::span<const std::byte> row) override;
    void update_row(Row row) override;
    void delete_row(const row_id_t& rid) override;
};
