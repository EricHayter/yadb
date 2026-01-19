find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(Parser
    Parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/Parser.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/Parser.h
)

flex_target(Lexer
    Lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/Lexer.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/Lexer.h
)

add_flex_bison_dependency(Lexer Parser)

add_library(sql_parser
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(sql_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
