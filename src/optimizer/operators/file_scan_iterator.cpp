#include "optimizer/operators/file_scan_iterator.h"
#include <optional>

FileScanIterator::~FileScanIterator()
{
    if (!is_closed_m) {
        table_iter_m->close();
    }
}

std::optional<std::vector<std::byte>> FileScanIterator::next()
{
    auto row = table_iter_m->next();
    if (!row.has_value())
        return std::nullopt;
    auto [rid, row_data] = row.value();
    std::vector<std::byte> res(row_data.begin(), row_data.end());
    return res;
}

void FileScanIterator::close()
{
    if (!is_closed_m) {
        table_iter_m->close();
        is_closed_m = true;
    }
}
