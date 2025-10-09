add_library(bptree_slotted_page
    base_page.cpp
    checksum.cpp
    mut_page.cpp
    page.cpp
)

target_include_directories(bptree_slotted_page PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(bptree_slotted_page
    PUBLIC yadb_config
    INTERFACE lib_compile_flags
)
