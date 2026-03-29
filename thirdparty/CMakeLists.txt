add_subdirectory(googletest EXCLUDE_FROM_ALL)
add_subdirectory(spdlog)
add_subdirectory(replxx)

# Mark spdlog include directories as SYSTEM to suppress warnings
get_target_property(SPDLOG_INCLUDE_DIRS spdlog INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(spdlog PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIRS}"
)
