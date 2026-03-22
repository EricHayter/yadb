add_subdirectory(operators)

add_library(yadb_optimizer
    external_sort.cpp
    optimizer.cpp
)

target_include_directories(yadb_optimizer PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_optimizer
    INTERFACE lib_compile_flags
    PUBLIC sql_parser
    PRIVATE
    yadb_storage_on_disk
    yadb_optimizer_operators
)
