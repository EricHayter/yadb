#pragma once

#include "optimizer/operators/iterator.h"
#include "table/table.h"

class FileScanIterator : public Iterator {
    public:
    FileScanIterator(Table& table);
    ~FileScanIterator() override;
    std::optional<std::vector<std::byte>> next() override;
    private:
    Table& table_m;
    bool is_closed_m = false;
};
