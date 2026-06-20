#pragma once

#include "optimizer/operators/iterator.h"
#include "table/table_cursor.h"

class FileScanIterator : public Iterator {
public:
    explicit FileScanIterator(TableCursor table_iter);

    Iterator& operator++() override;

private:
    void load_current();

    TableCursor table_iter_m;
};
