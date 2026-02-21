add_executable(external_sort_test external_sort_test.cpp)
target_link_libraries(external_sort_test
    PRIVATE
    yadb_storage_bptree
    yadb_optimizer
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(external_sort_test PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(external_sort_test)
