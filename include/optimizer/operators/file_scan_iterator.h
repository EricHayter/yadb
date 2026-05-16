#pragma once

#include "optimizer/operators/iterator.h"
#include "table/table_iterator.h"
#include <memory>

class FileScanIterator : public Iterator {
public:
    FileScanIterator(std::unique_ptr<TableIterator> table_iter);

    Iterator& operator++() override;

private:
    void load_current();

    std::unique_ptr<TableIterator> table_iter_m;
};
