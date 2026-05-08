add_executable(table_manager_test
    table_manager_test.cpp
)

target_link_libraries(table_manager_test
    PRIVATE yadb_table
    PRIVATE yadb_catalog
    PRIVATE yadb_core
    PRIVATE GTest::gtest_main
    INTERFACE lib_compile_flags
)

include(GoogleTest)
gtest_discover_tests(table_manager_test)
add_test(TableManagerTest table_manager_test)