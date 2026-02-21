add_library(yadb_catalog
    catalog.cpp
)

target_include_directories(yadb_catalog PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_catalog
    INTERFACE lib_compile_flags
    PRIVATE yadb_table
)
