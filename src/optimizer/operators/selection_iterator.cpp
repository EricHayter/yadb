#include "optimizer/operators/selection_iterator.h"
#include <optional>

SelectionIterator::SelectionIterator(std::unique_ptr<Iterator> in, SelectionFunction selection_func)
    : in_m(std::move(in))
    , selection_func_m(std::move(selection_func))
{}

SelectionIterator::~SelectionIterator() {
    if (!closed_m)
        in_m->close();
}

std::optional<std::vector<std::byte>> SelectionIterator::next() {
    auto data = in_m->next();
    if (!data.has_value())
        return std::nullopt;
    while (data && !selection_func_m(*data)) {
        data = in_m->next();
    }
    return data;
}

void SelectionIterator::close() {
    closed_m = true;
    in_m->close();
}
