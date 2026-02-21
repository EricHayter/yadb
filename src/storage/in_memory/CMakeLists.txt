# In-memory storage engine library
add_library(yadb_storage_in_memory
    in_memory_table.cpp
    in_memory_table_manager.cpp
)

target_include_directories(yadb_storage_in_memory
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(yadb_storage_in_memory
    INTERFACE lib_compile_flags
)
