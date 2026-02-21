add_library(yadb_optimizer
    external_sort.cpp
)

target_include_directories(yadb_optimizer PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_optimizer
    INTERFACE lib_compile_flags
    PRIVATE
    yadb_storage_bptree
)
