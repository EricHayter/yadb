add_subdirectory(buffer)
add_subdirectory(storage)

add_library(yadb_core INTERFACE)

target_link_libraries(yadb_core
    INTERFACE yadb_buffer
    INTERFACE yadb_storage
)
