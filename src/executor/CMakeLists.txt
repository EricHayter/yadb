add_library(yadb_executor
    executor.cpp
)

target_include_directories(yadb_executor PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_executor
    INTERFACE lib_compile_flags
    PRIVATE sql_parser
    PRIVATE yadb_catalog
    PRIVATE yadb_storage_in_memory
)
