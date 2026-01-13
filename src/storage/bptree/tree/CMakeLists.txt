add_library(bptree_tree
    bptree.cpp
    bptree_search.cpp
    bptree_insert.cpp
    bptree_delete.cpp
    bptree_internal.cpp
)

target_include_directories(bptree_tree PUBLIC ${CMAKE_SOURCE_DIR}/include/storage/bptree)

target_link_libraries(bptree_tree
    PUBLIC bptree_buffer_manager
    PUBLIC bptree_slotted_page
    INTERFACE lib_compile_flags
)
