add_library(bptree_disk_manager
    disk_manager.cpp
    disk_scheduler.cpp
)

target_include_directories(bptree_disk_manager PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(bptree_disk_manager
    PUBLIC bptree_slotted_page
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
