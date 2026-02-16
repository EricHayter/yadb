add_library(yadb_bptree_table
    bptree_table.cpp
    bptree_table_manager.cpp
)

target_include_directories(yadb_bptree_table PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_bptree_table
    INTERFACE lib_compile_flags
)
