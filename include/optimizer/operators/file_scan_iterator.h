#pragma once

#include "optimizer/operators/iterator.h"
#include "table/table_iterator.h"
#include <memory>

class FileScanIterator : public Iterator {
    public:
    FileScanIterator(std::unique_ptr<TableIterator> table_iter) : table_iter_m(std::move(table_iter)) {};
    ~FileScanIterator() override;
    std::optional<std::vector<std::byte>> next() override;
    private:
    std::unique_ptr<TableIterator> table_iter_m;
    bool is_closed_m = false;
};
