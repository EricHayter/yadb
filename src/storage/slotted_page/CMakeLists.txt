add_library(yadb_slotted_page
    page_format.cpp
    checksum.cpp
)

target_include_directories(yadb_slotted_page PUBLIC ${CMAKE_SOURCE_DIR}/include/storage)

target_link_libraries(yadb_slotted_page
    PRIVATE yadb_core
    PUBLIC yadb_buffer_manager
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
