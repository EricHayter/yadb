#include "executor/executor.h"
#include "table/table_manager.h"

Executor::Executor()
    : table_manager_m { std::make_unique<TableManager>() }
    , optimizer_m(*table_manager_m)
{
}

Executor::ExecutionResult Executor::execute(const SqlStmt& stmt)
{
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
    },
        stmt);
}

Executor::ExecutionResult Executor::execute(const SelectStmt& stmt)
{
    try {
        // Get execution iterator from optimizer
        auto iter = optimizer_m.get_execution_iterator(stmt);

        // Collect all rows
        std::vector<std::vector<std::byte>> result_rows;
        for (const auto& row : *iter) {
            result_rows.push_back(row);
        }

        // Get schema from table for result metadata
        auto table = table_manager_m->GetTable(stmt.table_name);
        if (!table)
            return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };
        const Schema& full_schema = table->GetSchema();

        // Build result schema based on selected columns
        Schema result_schema;
        if (stmt.select_all) {
            result_schema = full_schema;
        } else {
            // Map selected column names to schema
            for (const auto& col_name : stmt.columns) {
                for (const auto& attr : full_schema) {
                    if (attr.name == col_name) {
                        result_schema.push_back(attr);
                        break;
                    }
                }
            }
        }

        return ExecutionResult {
            .success = true,
            .rows = std::move(result_rows),
            .schema = std::move(result_schema)
        };

    } catch (const std::exception& e) {
        // Error during execution
        return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };
    }
}

Executor::ExecutionResult Executor::execute(const InsertStmt& stmt)
{
    auto table = table_manager_m->GetTable(stmt.table_name);
    if (!table)
        return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };

    try {
        table->insert_row(stmt.values);
        return ExecutionResult { .success = true, .rows = std::nullopt, .schema = std::nullopt };
    } catch (const std::exception&) {
        return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };
    }
}

Executor::ExecutionResult Executor::execute(const CreateTableStmt& stmt)
{
    // Default to InMemory table type
    bool success = table_manager_m->CreateTable(stmt.table_name, TableType::InMemory, stmt.columns);
    return ExecutionResult { .success = success, .rows = std::nullopt, .schema = std::nullopt };
}

Executor::ExecutionResult Executor::execute(const DropTableStmt& stmt)
{
    bool success = table_manager_m->DeleteTable(stmt.table_name);
    return ExecutionResult { .success = success, .rows = std::nullopt, .schema = std::nullopt };
}

Executor::ExecutionResult Executor::execute(const DeleteStmt& /*stmt*/)
{
    // TODO: Implement DELETE execution
    return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };
}

Executor::ExecutionResult Executor::execute(const UpdateStmt& /*stmt*/)
{
    // TODO: Implement UPDATE execution
    return ExecutionResult { .success = false, .rows = std::nullopt, .schema = std::nullopt };
}
