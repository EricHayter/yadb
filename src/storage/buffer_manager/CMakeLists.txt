add_library(yadb_buffer_manager
    page.cpp
    page_buffer_manager.cpp
    frame.cpp
    lru_k_replacer.cpp
)

target_include_directories(yadb_buffer_manager PUBLIC ${CMAKE_SOURCE_DIR}/include/storage)

target_link_libraries(yadb_buffer_manager
    PUBLIC yadb_disk_manager
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
