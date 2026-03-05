#pragma once

#include "optimizer/operators/iterator.h"
#include <functional>

class SelectionIterator : public Iterator {
    public:
    /* What is this going to need?
     * going to need an iterator
     * going to need a funciton of some sort I guess.
     * Schema
     * I guess the parameters? A vector mapping most likely
     */
    using SelectionFunction = std::function<bool(const std::vector<std::byte>&)>;

    SelectionIterator(Iterator& in, const SelectionFunction& selection_func)
        : in_m(in)
        , selection_func_m(selection_func)
    {}
    ~SelectionIterator();
    std::optional<std::vector<std::byte>> next() override;
    void close() override;

    private:
    bool closed_m{false};
    Iterator& in_m;
    SelectionFunction selection_func_m;
};
