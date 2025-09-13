add_subdirectory(buffer)
add_subdirectory(storage)
add_subdirectory(config)

add_library(yadb_core INTERFACE)

target_link_libraries(yadb_core
    INTERFACE yadb_buffer
    INTERFACE yadb_storage
    INTERFACE yadb_config
)
