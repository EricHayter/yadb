#pragma once

#include <iostream>
#include <cstdlib>

/**
 * Custom assertion macro for YADB
 *
 * YADB_ASSERT(condition, message)
 *   - Only active in debug builds (disabled when NDEBUG is defined)
 *   - Prints detailed error information including the condition, custom message,
 *     file location, line number, and function name
 *   - Aborts the program if the condition is false
 *
 * Usage:
 *   YADB_ASSERT(ptr != nullptr, "Pointer must not be null");
 *   YADB_ASSERT(count > 0, "Count must be positive");
 */

#ifdef NDEBUG
    #define YADB_ASSERT(condition, message) ((void)0)
#else
    #define YADB_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                yadb::detail::AssertionFailed(#condition, message, __FILE__, __LINE__, __func__); \
            } \
        } while (0)
#endif

namespace yadb::detail {
    /**
     * Called when an assertion fails
     *
     * This function prints detailed error information to stderr and aborts
     * the program. It should not be called directly - use YADB_ASSERT instead.
     */
    [[noreturn]] inline void AssertionFailed(
        const char* condition,
        const char* message,
        const char* file,
        int line,
        const char* func)
    {
        std::cerr << "\n=== Assertion Failed ===\n"
                  << "Condition: " << condition << "\n"
                  << "Message:   " << message << "\n"
                  << "Location:  " << file << ":" << line << "\n"
                  << "Function:  " << func << "\n"
                  << "========================\n" << std::flush;
        std::abort();
    }
}
