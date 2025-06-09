cmake_minimum_required(VERSION 3.25)

project(MyExample)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)
add_subdirectory(thirdparty)

enable_testing()

add_executable(
  yadb_test
  EXCLUDE_FROM_ALL
  tests/storage/disk/disk_manager_test.cpp src/storage/disk/disk_manager.cpp tests/storage/disk/disk_scheduler_test.cpp src/storage/disk/disk_scheduler.cpp
  tests/buffer/buffer_pool_manager_test.cpp
  	src/buffer/buffer_pool_manager.cpp src/buffer/lru_k_replacer.cpp
	src/buffer/page_guard.cpp src/buffer/frame_header.cpp
)
target_link_libraries(
  yadb_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(yadb_test)

