add_library(bptree_slotted_page
    page.cpp
    checksum.cpp
)

target_include_directories(bptree_slotted_page PUBLIC ${CMAKE_SOURCE_DIR}/include/storage/bptree)

target_link_libraries(bptree_slotted_page
    PUBLIC bptree_buffer_manager
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
