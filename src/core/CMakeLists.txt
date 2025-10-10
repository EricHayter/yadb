add_library(yadb_core
    shared_spinlock.cpp
)

target_include_directories(yadb_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
