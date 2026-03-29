add_library(yadb_common
    definitions.cpp
)

target_include_directories(yadb_common PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_common
    INTERFACE lib_compile_flags
    PRIVATE lib_strict_warnings
)
