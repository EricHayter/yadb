#pragma once

#include "Parser.h"
#include "catalog/catalog.h"
#include "table/table_manager.h"

class Executor {
    public:
    Executor();

    struct ExecutionResult {
        bool success;
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
    Catalog catalog_m;
};
