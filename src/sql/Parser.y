%require "3.2"
%language "c++"
%define api.value.type variant

%code requires {
    #include <string>
    #include <vector>
    #include <variant>
    #include <optional>
    #include <string_view>
    #include <memory>
    #include "common/definitions.h"

    // Forward declarations for recursive structures
    struct Condition;

    // Comparison operators
    enum class ComparisonOp {
        EQ,   // =
        NEQ,  // !=
        LT,   // <
        GT,   // >
        LE,   // <=
        GE    // >=
    };

    // Logical operators
    enum class LogicalOp {
        AND,
        OR
    };

    // An operand in a comparison can be a column name or a literal value
    struct Operand {
        std::variant<std::string, Value> data;
    };

    // Simple comparison: column_name op value
    struct Comparison {
        Operand left;
        ComparisonOp op;
        Operand right;
    };

    // Compound condition: condition AND/OR condition
    struct LogicalCondition {
        std::shared_ptr<Condition> left;
        LogicalOp op;
        std::shared_ptr<Condition> right;
    };

    // A condition can be either a simple comparison or a compound logical condition
    struct Condition {
        std::variant<Comparison, LogicalCondition> expr;
    };

    // AST node types
    struct SelectStmt {
        std::string table_name;
        std::vector<std::string> columns;
        bool select_all = false;
        std::shared_ptr<Condition> where_clause;
    };

    struct InsertStmt {
        std::string table_name;
        std::vector<std::string> columns;  // Optional: empty if not specified
        std::vector<Value> values;
    };

    struct CreateTableStmt {
        std::string table_name;
        Schema columns;
    };

    struct DropTableStmt {
        std::string table_name;
    };

    struct DeleteStmt {
        std::string table_name;
        bool has_where = false;
        std::shared_ptr<Condition> where_clause;
    };

    struct UpdateStmt {
        std::string table_name;
        std::vector<std::string> set_columns;  // Columns being updated
        bool has_where = false;
        std::shared_ptr<Condition> where_clause;
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
%type <Schema> column_def_list
%type <RelationAttribute> column_def
%type <DataType> data_type
%type <Value> value
%type <std::vector<Value>> value_list
%type <std::shared_ptr<Condition>> condition
%type <ComparisonOp> comparison_op
%type <Operand> operand

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
        stmt.where_clause = nullptr;
        $$ = stmt;
    }
    | SELECT column_list FROM table_name WHERE condition {
        SelectStmt stmt;
        stmt.select_all = $2;
        stmt.table_name = $4;
        stmt.where_clause = $6;
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
        stmt.values = $6;
        $$ = stmt;
    }
    | INSERT INTO table_name LPAREN column_list RPAREN VALUES LPAREN value_list RPAREN {
        InsertStmt stmt;
        stmt.table_name = $3;
        // TODO: capture column names from $5
        stmt.values = $9;
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
        stmt.where_clause = nullptr;
        $$ = stmt;
    }
    | DELETE FROM table_name WHERE condition {
        DeleteStmt stmt;
        stmt.table_name = $3;
        stmt.has_where = true;
        stmt.where_clause = $5;
        $$ = stmt;
    }
    ;

update_stmt:
    UPDATE table_name SET assignment_list {
        UpdateStmt stmt;
        stmt.table_name = $2;
        stmt.has_where = false;
        stmt.where_clause = nullptr;
        // assignment_list would populate set_columns (not captured yet)
        $$ = stmt;
    }
    | UPDATE table_name SET assignment_list WHERE condition {
        UpdateStmt stmt;
        stmt.table_name = $2;
        stmt.has_where = true;
        stmt.where_clause = $6;
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
        $$ = Schema{$1};
    }
    | column_def_list COMMA column_def {
        $1.push_back($3);
        $$ = $1;
    }
    ;

column_def:
    column_name data_type {
        RelationAttribute def;
        def.name = $1;
        def.type = $2;
        $$ = def;
    }
    ;

data_type:
    INTEGER     { $$ = DataType::INTEGER; }
    | TEXT      { $$ = DataType::TEXT; }
    ;

condition:
    operand comparison_op operand {
        auto cond = std::make_shared<Condition>();
        Comparison cmp;
        cmp.left = $1;
        cmp.op = $2;
        cmp.right = $3;
        cond->expr = cmp;
        $$ = cond;
    }
    | condition AND condition {
        auto cond = std::make_shared<Condition>();
        LogicalCondition logical;
        logical.left = $1;
        logical.op = LogicalOp::AND;
        logical.right = $3;
        cond->expr = logical;
        $$ = cond;
    }
    | condition OR condition {
        auto cond = std::make_shared<Condition>();
        LogicalCondition logical;
        logical.left = $1;
        logical.op = LogicalOp::OR;
        logical.right = $3;
        cond->expr = logical;
        $$ = cond;
    }
    | LPAREN condition RPAREN {
        $$ = $2;
    }
    ;

operand:
    column_name {
        Operand op;
        op.data = $1;
        $$ = op;
    }
    | value {
        Operand op;
        op.data = $1;
        $$ = op;
    }
    ;

comparison_op:
    EQ      { $$ = ComparisonOp::EQ; }
    | NEQ   { $$ = ComparisonOp::NEQ; }
    | LT    { $$ = ComparisonOp::LT; }
    | GT    { $$ = ComparisonOp::GT; }
    | LE    { $$ = ComparisonOp::LE; }
    | GE    { $$ = ComparisonOp::GE; }
    ;

value_list:
    value {
        $$ = std::vector<Value>{$1};
    }
    | value_list COMMA value {
        $1.push_back($3);
        $$ = $1;
    }
    ;

value:
    NUMBER      { $$ = Value{$1}; }
    | STRING    { $$ = Value{$1}; }
    | IDENTIFIER { $$ = Value{$1}; }
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
