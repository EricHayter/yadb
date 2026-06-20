#pragma once

#include "table/table_iterator.h"
#include <map>
#include <optional>
#include <vector>

class EphemeralTableIterator : public TableIterator {
public:
    EphemeralTableIterator(std::map<row_id_t, std::vector<std::byte>>& data);

    TableIterator::value_type operator*() const override;
    TableIterator& operator++() override;
    bool operator==(std::default_sentinel_t) const override;
    void seek(row_id_t rid) override;

private:
    void load_current();

    std::map<row_id_t, std::vector<std::byte>>& data_m;
    std::map<row_id_t, std::vector<std::byte>>::iterator iterator_m;

    // Caches the Row for the current map entry. The span inside points directly
    // at the entry's vector (no copy needed, unlike the disk iterator), and is
    // nullopt once iteration is exhausted.
    std::optional<Row> current_m;
};
