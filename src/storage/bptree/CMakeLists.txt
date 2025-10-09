add_subdirectory(buffer)
add_subdirectory(disk)
add_subdirectory(page)

add_library(bptree_storage_engine INTERFACE)

target_link_libraries(bptree_storage_engine
    INTERFACE bptree_buffer_manager
    INTERFACE bptree_slotted_page
    INTERFACE lib_compile_flags
)
