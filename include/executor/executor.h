#pragma once

#include "Parser.h"
#include "optimizer/optimizer.h"
#include "table/table_manager.h"
#include <optional>
#include <vector>

class Executor {
public:
    Executor();

    struct ExecutionResult {
        bool success;
        // For SELECT queries
        std::optional<std::vector<std::vector<std::byte>>> rows;
        std::optional<Schema> schema;
    };

    ExecutionResult execute(const SqlStmt& stmt);

private:
    ExecutionResult execute(const SelectStmt& stmt);
    ExecutionResult execute(const InsertStmt& stmt);
    ExecutionResult execute(const CreateTableStmt& stmt);
    ExecutionResult execute(const DropTableStmt& stmt);
    ExecutionResult execute(const DeleteStmt& stmt);
    ExecutionResult execute(const UpdateStmt& stmt);

private:
    std::unique_ptr<TableManager> table_manager_m;
    Optimizer optimizer_m;
};
