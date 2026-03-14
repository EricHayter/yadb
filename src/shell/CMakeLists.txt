add_executable(yadb-shell
    main.cpp
    result_printer.cpp
)

target_include_directories(yadb-shell PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(yadb-shell
    INTERFACE lib_compile_flags
    PUBLIC replxx
    PUBLIC sql_parser
    PUBLIC yadb_executor
)
