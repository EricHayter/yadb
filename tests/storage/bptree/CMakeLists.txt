add_executable(bptree_test
    disk/disk_manager_test.cpp
    disk/disk_scheduler_test.cpp
    buffer/page_buffer_manager_test.cpp
    page/checksum_test.cpp
    page/page_iterator_test.cpp
    # tree/bptree_test.cpp  # Disabled - B+ tree implementation in progress
)

target_link_libraries(bptree_test
    PRIVATE bptree_slotted_page
    PRIVATE bptree_buffer_manager
    PRIVATE bptree_storage_engine
    PRIVATE GTest::gtest_main
    INTERFACE lib_compile_flags
)

include(GoogleTest)
gtest_discover_tests(bptree_test)
add_test(BPTreeTest bptree_test)
