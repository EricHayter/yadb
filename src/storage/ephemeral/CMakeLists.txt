# Ephemeral storage engine library
add_library(yadb_storage_ephemeral
    ephemeral_table.cpp
    ephemeral_table_iterator.cpp
    ephemeral_table_factory.cpp
)

target_include_directories(yadb_storage_ephemeral
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(yadb_storage_ephemeral
    INTERFACE lib_compile_flags
    INTERFACE lib_strict_warnings
    PUBLIC yadb_table
)
