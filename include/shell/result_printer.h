#pragma once

#include "executor/executor.h"
#include <iostream>
#include <string>
#include <vector>

class ResultPrinter {
public:
    static void print(const Executor::ExecutionResult& result, std::ostream& out = std::cout);

private:
    // Table formatting helpers
    static std::vector<std::vector<std::string>> deserialize_rows(
        const std::vector<std::vector<std::byte>>& rows,
        const Schema& schema);

    static std::vector<size_t> calculate_column_widths(
        const std::vector<std::vector<std::string>>& string_rows,
        const Schema& schema);

    static void print_header_separator(
        const std::vector<size_t>& widths,
        std::ostream& out);

    static void print_row_separator(
        const std::vector<size_t>& widths,
        std::ostream& out);

    static void print_footer_separator(
        const std::vector<size_t>& widths,
        std::ostream& out);

    static void print_row(
        const std::vector<std::string>& row,
        const std::vector<size_t>& widths,
        std::ostream& out);

    static std::string value_to_string(
        const std::vector<std::byte>& row_bytes,
        const Schema& schema,
        size_t col_idx);
};
