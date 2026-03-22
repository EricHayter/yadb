#include "shell/result_printer.h"
#include "core/row_reader.h"
#include <algorithm>
#include <iomanip>

void ResultPrinter::print(const Executor::ExecutionResult& result, std::ostream& out)
{
    // Handle failures
    if (!result.success) {
        out << "Error: Query failed\n";
        return;
    }

    // Handle non-SELECT queries (INSERT, CREATE TABLE, etc.)
    if (!result.rows.has_value() || !result.schema.has_value()) {
        out << "Query OK\n";
        return;
    }

    const auto& rows = result.rows.value();
    const auto& schema = result.schema.value();

    // Empty result set
    if (rows.empty()) {
        out << "Empty set (0 rows)\n";
        return;
    }

    // Deserialize all rows to strings
    auto string_rows = deserialize_rows(rows, schema);

    // Calculate column widths
    auto widths = calculate_column_widths(string_rows, schema);

    // Print table
    print_header_separator(widths, out);

    // Print column names
    std::vector<std::string> header_row;
    for (const auto& attr : schema) {
        header_row.push_back(attr.name);
    }
    print_row(header_row, widths, out);

    print_row_separator(widths, out);

    // Print data rows
    for (const auto& row : string_rows) {
        print_row(row, widths, out);
    }

    print_footer_separator(widths, out);

    // Print summary
    out << rows.size() << " row" << (rows.size() == 1 ? "" : "s") << " in set\n";
}

std::vector<std::vector<std::string>> ResultPrinter::deserialize_rows(
    const std::vector<std::vector<std::byte>>& rows,
    const Schema& schema)
{
    std::vector<std::vector<std::string>> result;
    result.reserve(rows.size());

    for (const auto& row_bytes : rows) {
        std::vector<std::string> string_row;
        string_row.reserve(schema.size());

        for (size_t i = 0; i < schema.size(); ++i) {
            string_row.push_back(value_to_string(row_bytes, schema, i));
        }

        result.push_back(std::move(string_row));
    }

    return result;
}

std::string ResultPrinter::value_to_string(
    const std::vector<std::byte>& row_bytes,
    const Schema& schema,
    size_t col_idx)
{
    RowReader reader(row_bytes, schema);
    const auto& attr = schema[col_idx];

    if (attr.type == DataType::INTEGER) {
        return std::to_string(reader.Get<DataType::INTEGER>(col_idx));
    } else if (attr.type == DataType::TEXT) {
        return reader.Get<DataType::TEXT>(col_idx);
    }

    return "???"; // Unknown type
}

std::vector<size_t> ResultPrinter::calculate_column_widths(
    const std::vector<std::vector<std::string>>& string_rows,
    const Schema& schema)
{
    std::vector<size_t> widths;
    widths.reserve(schema.size());

    // Initialize with header widths
    for (const auto& attr : schema) {
        widths.push_back(attr.name.length());
    }

    // Update with data widths
    for (const auto& row : string_rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].length());
        }
    }

    return widths;
}

void ResultPrinter::print_header_separator(
    const std::vector<size_t>& widths,
    std::ostream& out)
{
    out << "┌";
    for (size_t i = 0; i < widths.size(); ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) {
            out << "─";
        }
        if (i < widths.size() - 1) {
            out << "┬";
        }
    }
    out << "┐\n";
}

void ResultPrinter::print_row_separator(
    const std::vector<size_t>& widths,
    std::ostream& out)
{
    out << "├";
    for (size_t i = 0; i < widths.size(); ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) {
            out << "─";
        }
        if (i < widths.size() - 1) {
            out << "┼";
        }
    }
    out << "┤\n";
}

void ResultPrinter::print_footer_separator(
    const std::vector<size_t>& widths,
    std::ostream& out)
{
    out << "└";
    for (size_t i = 0; i < widths.size(); ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) {
            out << "─";
        }
        if (i < widths.size() - 1) {
            out << "┴";
        }
    }
    out << "┘\n";
}

void ResultPrinter::print_row(
    const std::vector<std::string>& row,
    const std::vector<size_t>& widths,
    std::ostream& out)
{
    out << "│";
    for (size_t i = 0; i < row.size(); ++i) {
        out << " " << std::left << std::setw(widths[i]) << row[i] << " ";
        out << "│";
    }
    out << "\n";
}
