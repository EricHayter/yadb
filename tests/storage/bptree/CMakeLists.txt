add_executable(bptree_storage_test
    disk/disk_manager_test.cpp
    disk/disk_scheduler_test.cpp
    buffer/page_buffer_manager_test.cpp
    page/checksum_test.cpp
)

target_link_libraries(bptree_storage_test
    PRIVATE yadb_storage_bptree
    PRIVATE GTest::gtest_main
    INTERFACE lib_compile_flags
)

include(GoogleTest)
gtest_discover_tests(bptree_storage_test)
add_test(BPTreeStorageTest bptree_storage_test)
