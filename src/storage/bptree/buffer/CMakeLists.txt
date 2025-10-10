add_library(bptree_buffer_manager
    page_buffer_manager.cpp
    frame_header.cpp
    lru_k_replacer.cpp
)

target_include_directories(bptree_buffer_manager PUBLIC ${CMAKE_SOURCE_DIR}/include/storage/bptree)

target_link_libraries(bptree_buffer_manager
    PUBLIC bptree_disk_manager
    PUBLIC bptree_slotted_page
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
