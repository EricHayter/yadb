add_library(yadb_buffer
    page_buffer_manager.cpp
    frame_header.cpp
    lru_k_replacer.cpp
)

target_include_directories(yadb_buffer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(yadb_buffer
    PUBLIC yadb_storage
    PUBLIC spdlog::spdlog
    INTERFACE lib_compile_flags
)
