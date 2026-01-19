%require "3.2"
%language "c++"
%define api.value.type variant

%code requires {
    #include <string>
}

%code {
    #include <iostream>
    #include <memory>

    int yylex(yy::parser::semantic_type* yylval);
    void yyerror(const char *s);
}

%token SELECT FROM WHERE INSERT INTO VALUES CREATE TABLE DROP DELETE UPDATE SET
%token AND OR INTEGER TEXT
%token STAR LPAREN RPAREN COMMA SEMICOLON
%token EQ NEQ LT GT LE GE
%token <std::string> IDENTIFIER STRING
%token <int> NUMBER

%left OR
%left AND
%nonassoc EQ NEQ LT GT LE GE

%type <std::string> column_name table_name

%%

sql_stmt_list:
    %empty
    | sql_stmt_list sql_stmt
    ;

sql_stmt:
    select_stmt SEMICOLON       { std::cout << "Parsed SELECT statement\n"; }
    | insert_stmt SEMICOLON     { std::cout << "Parsed INSERT statement\n"; }
    | create_stmt SEMICOLON     { std::cout << "Parsed CREATE TABLE statement\n"; }
    | drop_stmt SEMICOLON       { std::cout << "Parsed DROP TABLE statement\n"; }
    | delete_stmt SEMICOLON     { std::cout << "Parsed DELETE statement\n"; }
    | update_stmt SEMICOLON     { std::cout << "Parsed UPDATE statement\n"; }
    ;

select_stmt:
    SELECT column_list FROM table_name
    | SELECT column_list FROM table_name WHERE condition
    ;

column_list:
    STAR
    | column_name
    | column_list COMMA column_name
    ;

insert_stmt:
    INSERT INTO table_name VALUES LPAREN value_list RPAREN
    | INSERT INTO table_name LPAREN column_list RPAREN VALUES LPAREN value_list RPAREN
    ;

create_stmt:
    CREATE TABLE table_name LPAREN column_def_list RPAREN
    ;

drop_stmt:
    DROP TABLE table_name
    ;

delete_stmt:
    DELETE FROM table_name
    | DELETE FROM table_name WHERE condition
    ;

update_stmt:
    UPDATE table_name SET assignment_list
    | UPDATE table_name SET assignment_list WHERE condition
    ;

assignment_list:
    assignment
    | assignment_list COMMA assignment
    ;

assignment:
    column_name EQ value
    ;

column_def_list:
    column_def
    | column_def_list COMMA column_def
    ;

column_def:
    column_name data_type
    ;

data_type:
    INTEGER
    | TEXT
    ;

condition:
    column_name comparison_op value
    | condition AND condition
    | condition OR condition
    | LPAREN condition RPAREN
    ;

comparison_op:
    EQ | NEQ | LT | GT | LE | GE
    ;

value_list:
    value
    | value_list COMMA value
    ;

value:
    NUMBER
    | STRING
    | IDENTIFIER
    ;

column_name:
    IDENTIFIER      { $$ = $1; }
    ;

table_name:
    IDENTIFIER      { $$ = $1; }
    ;

%%

void yyerror(const char *s) {
    std::cerr << "Parse error: " << s << std::endl;
}

void yy::parser::error(const std::string& msg) {
    std::cerr << "Parse error: " << msg << std::endl;
}

// Forward declarations from flex
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

extern "C" int parse_sql_string(const char *str) {
    YY_BUFFER_STATE buffer = yy_scan_string(str);
    yy::parser parser;
    int result = parser.parse();
    yy_delete_buffer(buffer);
    return result;
}
