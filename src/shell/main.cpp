#include "Parser.h"
#include "executor/executor.h"
#include "replxx.hxx"
#include "shell/result_printer.h"
#include <errno.h>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using Replxx = replxx::Replxx;
using namespace replxx::color;

// not sure if this should really be optional at all
std::optional<std::vector<SqlStmt>> parse_sql_string(std::string_view input);

void display_results(Executor::ExecutionResult res)
{
    ResultPrinter::print(res);
}

int main(int argc, char** argv)
{
    Executor executor {};

    // Check for -c flag to execute a command string
    if (argc >= 3 && std::string(argv[1]) == "-c") {
        std::string query = argv[2];

        auto result = parse_sql_string(query);
        if (!result) {
            std::cerr << "Invalid SQL\n";
            return 1;
        }

        for (const SqlStmt& stmt : *result) {
            auto exec_res = executor.execute(stmt);
            display_results(exec_res);
        }

        return 0;
    }

    // Interactive mode
    std::string prompt = "\x1b[1;32myadb\x1b[0m> ";
    Replxx rx;

    std::cout
        << "Weclome to Yet Another Database 1.0\n"
        << "Type \"help\" for help\n";

    for (;;) {
        // display the prompt and retrieve input from the user
        char const* cinput { nullptr };

        do {
            cinput = rx.input(prompt);
        } while ((cinput == nullptr) && (errno == EAGAIN));

        if (cinput == nullptr) {
            break;
        }

        // change cinput into a std::string
        // easier to manipulate
        std::string input { cinput };

        if (input.empty())
            continue;

        if (input == "quit" || input == "exit") {
            rx.history_add(input);
            break;
        }

        rx.history_add(input);

        auto result = parse_sql_string(input);
        if (!result) {
            std::cout << "Unknown command or invalid SQL\n";
            continue;
        }

        std::cout << std::format("Recieved {} queries\n", result->size());

        for (const SqlStmt& stmt : *result) {
            auto exec_res = executor.execute(stmt);
            display_results(exec_res);
        }
    }
}
