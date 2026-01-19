%require "3.2"
%language "c++"
%define api.value.type variant

%code requires {
    #include <string>
    #include <vector>
    #include <variant>
    #include <optional>
    #include <string_view>

    // AST node types
    struct SelectStmt {
        std::string table_name;
        std::vector<std::string> columns;
        bool select_all = false;
    };

    struct InsertStmt {
        std::string table_name;
        std::vector<std::string> columns;  // Optional: empty if not specified
        // Values are represented as strings for now
        std::vector<std::string> values;
    };

    struct ColumnDef {
        std::string name;
        std::string type;  // "INTEGER" or "TEXT"
    };

    struct CreateTableStmt {
        std::string table_name;
        std::vector<ColumnDef> columns;
    };

    struct DropTableStmt {
        std::string table_name;
    };

    struct DeleteStmt {
        std::string table_name;
        bool has_where = false;
        // WHERE condition details can be added later
    };

    struct UpdateStmt {
        std::string table_name;
        std::vector<std::string> set_columns;  // Columns being updated
        bool has_where = false;
        // WHERE condition and values can be added later
    };

    using SqlStmt = std::variant<
        SelectStmt,
        InsertStmt,
        CreateTableStmt,
        DropTableStmt,
        DeleteStmt,
        UpdateStmt
    >;
}

%parse-param { std::vector<SqlStmt>& result }

%code {
    #include <iostream>

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
%type <SelectStmt> select_stmt
%type <InsertStmt> insert_stmt
%type <CreateTableStmt> create_stmt
%type <DropTableStmt> drop_stmt
%type <DeleteStmt> delete_stmt
%type <UpdateStmt> update_stmt
%type <bool> column_list
%type <std::vector<ColumnDef>> column_def_list
%type <ColumnDef> column_def
%type <std::string> data_type

%%

sql_stmt_list:
    %empty
    | sql_stmt_list sql_stmt
    ;

sql_stmt:
    select_stmt SEMICOLON       { result.push_back($1); }
    | insert_stmt SEMICOLON     { result.push_back($1); }
    | create_stmt SEMICOLON     { result.push_back($1); }
    | drop_stmt SEMICOLON       { result.push_back($1); }
    | delete_stmt SEMICOLON     { result.push_back($1); }
    | update_stmt SEMICOLON     { result.push_back($1); }
    ;

select_stmt:
    SELECT column_list FROM table_name {
        SelectStmt stmt;
        stmt.select_all = $2;
        stmt.table_name = $4;
        $$ = stmt;
    }
    | SELECT column_list FROM table_name WHERE condition {
        SelectStmt stmt;
        stmt.select_all = $2;
        stmt.table_name = $4;
        $$ = stmt;
    }
    ;

column_list:
    STAR                        { $$ = true; }
    | column_name               { $$ = false; }
    | column_list COMMA column_name { $$ = false; }
    ;

insert_stmt:
    INSERT INTO table_name VALUES LPAREN value_list RPAREN {
        InsertStmt stmt;
        stmt.table_name = $3;
        // value_list values would go here (not captured yet)
        $$ = stmt;
    }
    | INSERT INTO table_name LPAREN column_list RPAREN VALUES LPAREN value_list RPAREN {
        InsertStmt stmt;
        stmt.table_name = $3;
        // columns and values would go here (not captured yet)
        $$ = stmt;
    }
    ;

create_stmt:
    CREATE TABLE table_name LPAREN column_def_list RPAREN {
        CreateTableStmt stmt;
        stmt.table_name = $3;
        stmt.columns = $5;
        $$ = stmt;
    }
    ;

drop_stmt:
    DROP TABLE table_name {
        DropTableStmt stmt;
        stmt.table_name = $3;
        $$ = stmt;
    }
    ;

delete_stmt:
    DELETE FROM table_name {
        DeleteStmt stmt;
        stmt.table_name = $3;
        stmt.has_where = false;
        $$ = stmt;
    }
    | DELETE FROM table_name WHERE condition {
        DeleteStmt stmt;
        stmt.table_name = $3;
        stmt.has_where = true;
        $$ = stmt;
    }
    ;

update_stmt:
    UPDATE table_name SET assignment_list {
        UpdateStmt stmt;
        stmt.table_name = $2;
        stmt.has_where = false;
        // assignment_list would populate set_columns (not captured yet)
        $$ = stmt;
    }
    | UPDATE table_name SET assignment_list WHERE condition {
        UpdateStmt stmt;
        stmt.table_name = $2;
        stmt.has_where = true;
        // assignment_list would populate set_columns (not captured yet)
        $$ = stmt;
    }
    ;

assignment_list:
    assignment
    | assignment_list COMMA assignment
    ;

assignment:
    column_name EQ value
    ;

column_def_list:
    column_def {
        $$ = std::vector<ColumnDef>{$1};
    }
    | column_def_list COMMA column_def {
        $1.push_back($3);
        $$ = $1;
    }
    ;

column_def:
    column_name data_type {
        ColumnDef def;
        def.name = $1;
        def.type = $2;
        $$ = def;
    }
    ;

data_type:
    INTEGER     { $$ = "INTEGER"; }
    | TEXT      { $$ = "TEXT"; }
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
    // Silently ignore parse errors
}

void yy::parser::error(const std::string& msg) {
    // Silently ignore parse errors
}

// Forward declarations from flex
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

std::optional<std::vector<SqlStmt>> parse_sql_string(std::string_view str) {
    std::vector<SqlStmt> result;

    YY_BUFFER_STATE buffer = yy_scan_string(str.data());
    yy::parser parser(result);
    int ret = parser.parse();
    yy_delete_buffer(buffer);

    if (ret == 0) {
        return result;
    }
    return std::nullopt;
}
