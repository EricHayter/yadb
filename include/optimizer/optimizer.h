#pragma once

#include "Parser.h"
#include "optimizer/operators/iterator.h"
#include "table/table_manager.h"
#include <memory>

class Optimizer {
public:
    explicit Optimizer(TableManager& table_manager);

    std::unique_ptr<Iterator> get_execution_iterator(const SelectStmt& stmt);

private:
    TableManager& table_manager_m;
};
