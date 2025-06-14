cmake_minimum_required(VERSION 3.25)

project(yadb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
