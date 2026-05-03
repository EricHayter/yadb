#pragma once

#include "table/table_iterator.h"
#include <map>
#include <vector>

class EphemeralTableIterator : public TableIterator {
public:
    EphemeralTableIterator(std::map<row_id_t, std::vector<std::byte>>& data);
    ~EphemeralTableIterator() override;

    std::optional<Row> next() override;
    void seek(row_id_t rid) override;
    void close() override;

private:
    std::map<row_id_t, std::vector<std::byte>>& data_m;
    std::map<row_id_t, std::vector<std::byte>>::iterator iterator_m;
    bool closed_m = false;
};
