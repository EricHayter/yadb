add_library(yadb_config
    config.cpp
)

target_include_directories(yadb_config PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb_config
    PUBLIC spdlog::spdlog
    INTERFACE lib_compile_flags
)
