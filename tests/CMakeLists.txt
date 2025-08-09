add_executable(
    yadb_test
    storage/disk/disk_manager_test.cpp
    storage/disk/disk_scheduler_test.cpp
    buffer/page_buffer_manager_test.cpp
    storage/page/crc_test.cpp
)
target_link_libraries(
    yadb_test
    PRIVATE yadb_core
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(yadb_test)

