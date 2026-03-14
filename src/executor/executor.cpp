#include "executor/executor.h"
#include "catalog/catalog.h"
#include "storage/in_memory/in_memory_table_manager.h"

Executor::Executor()
    : table_manager_m{ std::make_unique<InMemoryTableManager>() }
    , catalog_m(*table_manager_m)
{
}

Executor::ExecutionResult Executor::execute(const SqlStmt& stmt) {
    return std::visit([this](auto&& s) -> ExecutionResult {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SelectStmt>)
            return this->execute(s);
        else if constexpr (std::is_same_v<T, InsertStmt>)
            return this->execute(s);
        else if constexpr (std::is_same_v<T, CreateTableStmt>)
            return this->execute(s);
        else if constexpr (std::is_same_v<T, DropTableStmt>)
            return this->execute(s);
        else if constexpr (std::is_same_v<T, DeleteStmt>)
            return this->execute(s);
        else if constexpr (std::is_same_v<T, UpdateStmt>)
            return this->execute(s);
    }, stmt);
}

Executor::ExecutionResult Executor::execute(const SelectStmt& stmt) {
    // TODO: Implement SELECT execution
    return ExecutionResult{};
}

Executor::ExecutionResult Executor::execute(const InsertStmt& stmt) {
    // TODO: Implement INSERT execution
    return ExecutionResult{};
}

Executor::ExecutionResult Executor::execute(const CreateTableStmt& stmt) {
    ExecutionResult res;
    res.success = catalog_m.AddTable(stmt.table_name, stmt.columns);
    return res;
}

Executor::ExecutionResult Executor::execute(const DropTableStmt& stmt) {
    ExecutionResult res;
    res.success = catalog_m.RemoveTable(stmt.table_name);
    return res;
}

Executor::ExecutionResult Executor::execute(const DeleteStmt& stmt) {
    // TODO: Implement DELETE execution
    return ExecutionResult{};
}

Executor::ExecutionResult Executor::execute(const UpdateStmt& stmt) {
    // TODO: Implement UPDATE execution
    return ExecutionResult{};
}
