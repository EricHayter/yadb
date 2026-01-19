#include <iostream>
#include <string>
#include <istream>
#include <sstream>

#include "replxx.hxx"

#include "Parser.h"

using Replxx = replxx::Replxx;
using namespace replxx::color;


extern "C" {
      int parse_sql_string(const char* input);
}

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

        int result = parse_sql_string(input.c_str());
        if (result != 0) {
            std::cerr << "Parse failed\n";
        }
    }
}
