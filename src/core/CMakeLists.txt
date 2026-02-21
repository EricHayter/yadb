add_library(yadb_core
    shared_spinlock.cpp
    row_builder.cpp
    row_reader.cpp
)

target_include_directories(yadb_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_core
    INTERFACE lib_compile_flags
    PRIVATE yadb_common
)
