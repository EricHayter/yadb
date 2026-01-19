cmake_minimum_required(VERSION 3.25)

project(yadb LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(lib_compile_flags INTERFACE)

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

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
