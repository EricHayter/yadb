add_executable(shared_spinlock_test
    shared_spinlock_test.cpp
 )

target_link_libraries(shared_spinlock_test
    PRIVATE yadb_core
    PRIVATE GTest::gtest_main
    INTERFACE lib_compile_flags
)

include(GoogleTest)
gtest_discover_tests(shared_spinlock_test)
add_test(SharedSpinLockTest shared_spinlock_test)
