#include "optimizer/operators/file_scan_iterator.h"

FileScanIterator::FileScanIterator(std::unique_ptr<TableIterator> table_iter)
    : table_iter_m(std::move(table_iter))
{
    load_current();
}

Iterator& FileScanIterator::operator++()
{
    ++(*table_iter_m);
    load_current();
    return *this;
}

void FileScanIterator::load_current()
{
    if (*table_iter_m == std::default_sentinel_t {}) {
        current_m = std::nullopt;
        return;
    }
    const auto& [row_id, row_data] = **table_iter_m;
    current_m = std::vector<std::byte>(row_data.begin(), row_data.end());
}
