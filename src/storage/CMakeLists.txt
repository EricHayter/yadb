message(${CMAKE_SOURCE_DIR})
add_library(yadb_storage
    disk/disk_manager.cpp
    disk/disk_scheduler.cpp
    page/base_page.cpp
    page/mut_page.cpp
    page/page.cpp
    page/checksum.cpp
)
target_include_directories(yadb_storage PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(yadb_storage
    PRIVATE spdlog::spdlog
    INTERFACE lib_compile_flags
)
