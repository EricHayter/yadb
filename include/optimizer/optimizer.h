#pragma once

#include <memory>
#include "optimizer/operators/iterator.h"
#include "Parser.h"

class Optimizer {
    public:
    // This is going to need to have access to the metadata for each of the
    // tables... (can exapnd this with the catalog once that's ready).

    std::unique_ptr<Iterator> get_execution_iterator(const SelectStmt& stmt);
};
