#include "executor/executor.h"
#include "catalog/catalog.h"
#include "table/table_manager.h"

Executor::Executor()
    : table_manager_m{ std::make_unique<TableManager>() }
    , catalog_m(*table_manager_m)
    , optimizer_m(*table_manager_m)
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
    try {
        // Get execution iterator from optimizer
        auto iter = optimizer_m.get_execution_iterator(stmt);

        // Collect all rows
        std::vector<std::vector<std::byte>> result_rows;
        while (true) {
            auto row = iter->next();
            if (!row.has_value()) {
                break;
            }
            result_rows.push_back(std::move(row.value()));
        }

        // Close iterator
        iter->close();

        // Get schema from table for result metadata
        auto table = table_manager_m->GetTable(stmt.table_name);
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

        return ExecutionResult{
            .success = true,
            .rows = std::move(result_rows),
            .schema = std::move(result_schema)
        };

    } catch (const std::exception& e) {
        // Error during execution
        return ExecutionResult{ .success = false };
    }
}

Executor::ExecutionResult Executor::execute(const InsertStmt& stmt) {
    // Check table exists
    if (!table_manager_m->TableExists(stmt.table_name)) {
        return ExecutionResult{ .success = false };
    }

    // Get table (contains schema)
    auto table = table_manager_m->GetTable(stmt.table_name);
    if (!table) {
        return ExecutionResult{ .success = false };
    }

    // Type-safe insert with validation
    try {
        table->insert_row(stmt.values);
        return ExecutionResult{ .success = true };
    } catch (const std::exception& e) {
        // TODO: Add error message field to ExecutionResult
        // For now, just return failure
        return ExecutionResult{ .success = false };
    }
}

Executor::ExecutionResult Executor::execute(const CreateTableStmt& stmt) {
    ExecutionResult res;
    // Default to InMemory table type
    res.success = catalog_m.AddTable(stmt.table_name, TableType::InMemory, stmt.columns);
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
