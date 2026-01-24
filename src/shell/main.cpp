#include <errno.h>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Parser.h"
#include "replxx.hxx"

using Replxx = replxx::Replxx;
using namespace replxx::color;

std::optional<std::vector<SqlStmt>> parse_sql_string(std::string_view input);

int main(int argc, char **argv) {
    std::string prompt = "\x1b[1;32myadb\x1b[0m> ";
    Replxx rx;

    std::cout
        << "Weclome to Yet Another Database 1.0\n"
        << "Type \"help\" for help\n";

    for (;;) {
		// display the prompt and retrieve input from the user
		char const* cinput{ nullptr };

		do {
			cinput = rx.input(prompt);
		} while ( ( cinput == nullptr ) && ( errno == EAGAIN ) );

		if (cinput == nullptr) {
			break;
		}

		// change cinput into a std::string
		// easier to manipulate
		std::string input {cinput};

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
    }
}
