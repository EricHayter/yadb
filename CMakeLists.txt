cmake_minimum_required(VERSION 3.25)

project(yadb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable compile options for code coverage with gcov" OFF)
function(enable_coverage target)
    if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message("Adding code coverage compilation flags to: ${target}")
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
