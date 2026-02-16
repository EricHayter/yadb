add_library(yadb_table
    ./bptree_table.cpp
)

target_include_directories(yadb_table PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_table
    INTERFACE lib_compile_flags
)
