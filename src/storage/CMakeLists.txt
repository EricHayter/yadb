message(${CMAKE_SOURCE_DIR})
add_library(yadb_storage
    disk/disk_manager.cpp
    disk/disk_scheduler.cpp
)
target_include_directories(yadb_storage PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(yadb_storage
    PUBLIC spdlog::spdlog
    INTERFACE lib_compile_flags
)
