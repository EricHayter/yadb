add_library(yadb_optimizer_operators
    iterator.cpp
    file_scan_iterator.cpp
    projection_iterator.cpp
    selection_iterator.cpp
)

target_include_directories(yadb_optimizer_operators PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_optimizer_operators
    INTERFACE lib_compile_flags
    PRIVATE
    yadb_core
    yadb_common
    yadb_table
)
