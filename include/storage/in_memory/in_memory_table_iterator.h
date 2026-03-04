#pragma once

#include "table/table_iterator.h"
#include <map>
#include <vector>

class InMemoryTableIterator : public TableIterator {
public:
    InMemoryTableIterator(std::map<row_id_t, std::vector<std::byte>>& data);
    ~InMemoryTableIterator() override;

    std::optional<Expected<Row, TableError>> next() override;
    std::optional<TableError> seek(row_id_t rid) override;
    void close() override;

private:
    std::map<row_id_t, std::vector<std::byte>>& data_m;
    std::map<row_id_t, std::vector<std::byte>>::iterator iterator_m;
    bool closed_m = false;
};
