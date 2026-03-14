#include "optimizer/optimizer.h"
#include "optimizer/operators/file_scan_iterator.h"
#include "optimizer/operators/projection_iterator.h"
#include <stdexcept>
#include <format>

Optimizer::Optimizer(TableManager& table_manager)
    : table_manager_m(table_manager)
{
}

std::unique_ptr<Iterator> Optimizer::get_execution_iterator(const SelectStmt& stmt) {
    // Check table exists
    if (!table_manager_m.TableExists(stmt.table_name)) {
        throw std::runtime_error(std::format("Table '{}' does not exist", stmt.table_name));
    }

    // Get table (contains schema)
    auto table = table_manager_m.GetTable(stmt.table_name);
    if (!table) {
        throw std::runtime_error(std::format("Failed to get table '{}'", stmt.table_name));
    }

    const Schema& schema = table->GetSchema();

    // Create base FileScanIterator from table's iterator
    std::unique_ptr<Iterator> iter = std::make_unique<FileScanIterator>(table->iter());

    // If SELECT *, return file scan iterator as-is
    if (stmt.select_all) {
        return iter;
    }

    // Otherwise, create ProjectionIterator for selected columns
    // Map column names to indices
    std::vector<std::size_t> selected_indices;
    selected_indices.reserve(stmt.columns.size());

    for (const auto& col_name : stmt.columns) {
        // Find column index in schema
        bool found = false;
        for (std::size_t i = 0; i < schema.size(); ++i) {
            if (schema[i].name == col_name) {
                selected_indices.push_back(i);
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error(
                std::format("Column '{}' does not exist in table '{}'", col_name, stmt.table_name)
            );
        }
    }

    // Wrap with ProjectionIterator
    return std::make_unique<ProjectionIterator>(std::move(iter), schema, std::move(selected_indices));
}
