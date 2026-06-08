add_library(yadb_table
    table.cpp
    table_handle.cpp
    table_iterator.cpp
    table_manager.cpp
)

target_include_directories(yadb_table PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_table
    INTERFACE lib_compile_flags
    INTERFACE lib_strict_warnings
    PRIVATE yadb_common
    PRIVATE yadb_core
    PRIVATE yadb_catalog
    PRIVATE yadb_storage_ephemeral
    PRIVATE yadb_storage_on_disk
)
