cmake_minimum_required(VERSION 3.25)

project(yadb LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(lib_compile_flags INTERFACE)

# Basic warning flags for all code (including third-party)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lib_compile_flags INTERFACE
        -Wall                    # Enable most warnings
        -Wextra                  # Enable extra warnings
    )
endif()

# Strict warning flags for our code only
add_library(lib_strict_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lib_strict_warnings INTERFACE
        -Wpedantic               # Warn on language extensions
        -Werror                  # Treat warnings as errors
        -Wshadow                 # Warn on variable shadowing
        -Wnon-virtual-dtor       # Warn on non-virtual destructors
        -Wold-style-cast         # Warn on C-style casts
        -Wcast-align             # Warn on alignment issues
        -Wunused                 # Warn on unused variables/functions
        -Woverloaded-virtual     # Warn on overridden virtuals
        -Wconversion             # Warn on implicit conversions
        -Wsign-conversion        # Warn on sign conversions
        -Wnull-dereference       # Warn on potential null dereference
        -Wdouble-promotion       # Warn on float to double promotion
        -Wformat=2               # Warn on printf format issues
        # Suppress warnings from system/third-party headers
        -Wsystem-headers         # But first enable...
        -Wno-system-headers      # ...then disable for system headers
    )

    # Additional GCC-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(lib_strict_warnings INTERFACE
            -Wmisleading-indentation  # Warn on misleading indentation
            -Wduplicated-cond         # Warn on duplicated conditions
            -Wduplicated-branches     # Warn on duplicated branches
            -Wlogical-op              # Warn on logical operation issues
            -Wuseless-cast            # Warn on useless casts
        )
    endif()
endif()

# Enable sanitizers for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(ENABLE_ASAN "Enable AddressSanitizer in Debug builds" ON)
    option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer in Debug builds" ON)

    if(ENABLE_ASAN)
        target_compile_options(lib_compile_flags INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(lib_compile_flags INTERFACE -fsanitize=address)
    endif()

    if(ENABLE_UBSAN)
        target_compile_options(lib_compile_flags INTERFACE -fsanitize=undefined -fno-omit-frame-pointer)
        target_link_options(lib_compile_flags INTERFACE -fsanitize=undefined)
    endif()
endif()

option(ENABLE_COVERAGE "Enable compile options for code coverage with gcov" OFF)
if (ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lib_compile_flags INTERFACE --coverage -O0 -g)
        target_link_options(lib_compile_flags INTERFACE --coverage)
    else()
        message("GCov compile flags not supported by compiler")
    endif()
endif()

option(ENABLE_TSAN "Enable compile flags for thread sanatizer" OFF)
if (ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lib_compile_flags INTERFACE -fsanitize=thread -g)
        target_link_options(lib_compile_flags INTERFACE -fsanitize=thread)
    else()
        message("Thread sanitizer compile flags not supported by compiler")
    endif()
endif()


# Disable IWYU on thirdparty (if using)
set(SAVED_IWYU ${CMAKE_CXX_INCLUDE_WHAT_YOU_USE})
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "")
add_subdirectory(thirdparty)
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${SAVED_IWYU})

add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
