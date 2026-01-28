add_library(yadb_disk_manager
    disk_manager.cpp
    disk_scheduler.cpp
)

target_include_directories(yadb_disk_manager PUBLIC ${CMAKE_SOURCE_DIR}/include/storage)

target_link_libraries(yadb_disk_manager
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
