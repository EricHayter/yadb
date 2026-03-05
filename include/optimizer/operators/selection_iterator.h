#pragma once

#include "optimizer/operators/iterator.h"
#include <functional>
#include <memory>

class SelectionIterator : public Iterator {
    public:
    /* What is this going to need?
     * going to need an iterator
     * going to need a funciton of some sort I guess.
     * Schema
     * I guess the parameters? A vector mapping most likely
     */
    using SelectionFunction = std::function<bool(const std::vector<std::byte>&)>;

    SelectionIterator(std::unique_ptr<Iterator> in, SelectionFunction selection_func);
    ~SelectionIterator();
    std::optional<std::vector<std::byte>> next() override;
    void close() override;

    private:
    bool closed_m{false};
    std::unique_ptr<Iterator> in_m;
    SelectionFunction selection_func_m;
};
