add_library(yadb_common
    definitions.cpp
)

target_include_directories(yadb_common PUBLIC ${CMAKE_SOURCE_DIR}/include)
