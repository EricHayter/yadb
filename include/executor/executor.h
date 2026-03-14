#pragma once

#include "Parser.h"

class Executor {
    public:
    struct ExecutionResult {

    };

    ExecutionResult execute(const SqlStmt& stmt);

    private:
    void execute(const SelectStmt& stmt);
    void execute(const InsertStmt& stmt);
    void execute(const CreateTableStmt& stmt);
    void execute(const DropTableStmt& stmt);
    void execute(const DeleteStmt& stmt);
    void execute(const UpdateStmt& stmt);
};
