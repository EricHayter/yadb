# Unified B+ tree storage engine library
add_library(yadb_storage_bptree
    # Disk I/O
    disk/disk_manager.cpp
    disk/disk_scheduler.cpp

    # Buffer management
    buffer_manager/page.cpp
    buffer_manager/frame.cpp
    buffer_manager/lru_k_replacer.cpp
    buffer_manager/page_buffer_manager.cpp

    # Page format
    page/page_format.cpp
    page/checksum.cpp

    # Table implementation
    table/bptree_table.cpp
    table/bptree_table_manager.cpp
)

target_include_directories(yadb_storage_bptree
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(yadb_storage_bptree
    PUBLIC yadb_config
    PRIVATE yadb_core
    INTERFACE lib_compile_flags
)
