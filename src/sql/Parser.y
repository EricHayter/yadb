%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *s);
%}

%union {
    int ival;
    char *sval;
}

%token SELECT FROM WHERE INSERT INTO VALUES CREATE TABLE DROP DELETE UPDATE SET
%token AND OR INTEGER TEXT
%token STAR LPAREN RPAREN COMMA SEMICOLON
%token EQ NEQ LT GT LE GE
%token <sval> IDENTIFIER STRING
%token <ival> NUMBER

%left OR
%left AND
%nonassoc EQ NEQ LT GT LE GE

%type <sval> column_name table_name

%%

sql_stmt_list:
    %empty
    | sql_stmt_list sql_stmt
    ;

sql_stmt:
    select_stmt SEMICOLON       { printf("Parsed SELECT statement\n"); }
    | insert_stmt SEMICOLON     { printf("Parsed INSERT statement\n"); }
    | create_stmt SEMICOLON     { printf("Parsed CREATE TABLE statement\n"); }
    | drop_stmt SEMICOLON       { printf("Parsed DROP TABLE statement\n"); }
    | delete_stmt SEMICOLON     { printf("Parsed DELETE statement\n"); }
    | update_stmt SEMICOLON     { printf("Parsed UPDATE statement\n"); }
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
    | TABLE         { $$ = strdup("TABLE"); }
    | TEXT          { $$ = strdup("TEXT"); }
    | INTEGER       { $$ = strdup("INTEGER"); }
    ;

table_name:
    IDENTIFIER      { $$ = $1; }
    | TABLE         { $$ = strdup("TABLE"); }
    | TEXT          { $$ = strdup("TEXT"); }
    | INTEGER       { $$ = strdup("INTEGER"); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

// Forward declarations from flex
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

int parse_sql_string(const char *str) {
    YY_BUFFER_STATE buffer = yy_scan_string(str);
    int result = yyparse();
    yy_delete_buffer(buffer);
    return result;
}
