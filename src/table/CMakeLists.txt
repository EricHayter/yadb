add_library(yadb_table
    table.cpp
    table_iterator.cpp
    table_manager.cpp
)

target_include_directories(yadb_table PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_table
    INTERFACE lib_compile_flags
    PRIVATE yadb_common
    PRIVATE yadb_core
    PRIVATE yadb_storage_in_memory
    PRIVATE yadb_storage_on_disk
)
