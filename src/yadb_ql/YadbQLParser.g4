parser grammar YadbQLParser;

options {
    tokenVocab = YadbQLLexer;
}

parse
    : sql_stmt_list EOF
;

sql_stmt_list
    : SCOL* sql_stmt (SCOL+ sql_stmt)* SCOL*
;

sql_stmt
	: create_table_stmt
	| insert_stmt
	| delete_stmt
	| select_stmt
	| drop_table_stmt
;

create_table_stmt
	: CREATE_ TABLE_ table_name OPEN_PAR column_def (COMMA column_def)* CLOSE_PAR
;

table_name
	: any_name
;

insert_stmt
	: INSERT_ INTO_ table_name VALUES_ value_row (COMMA value_row)*
;

value_row
    : OPEN_PAR expr (COMMA expr)* CLOSE_PAR
;

// TODO expand this to something else
// SPECIFICALLY WITH THE OPERATORS just do equality for now as a POC
expr
	: literal_value
	| expr (
		EQ
		| NOT_EQ1
	) expr
;

literal_value
    : NUMERIC_LITERAL
    | STRING_LITERAL
    | NULL_
    | TRUE_
    | FALSE_
    | CURRENT_TIME_
    | CURRENT_DATE_
    | CURRENT_TIMESTAMP_
;

delete_stmt
	: DELETE_ FROM_ table_name WHERE_ expr
;

select_stmt
	: 
;

drop_table_stmt
	: DROP_ TABLE_ table_name
;

column_def
	: column_name column_type
;

column_name
	: any_name
;

column_type
	: any_name
;

any_name
    : IDENTIFIER
    | STRING_LITERAL
    | OPEN_PAR any_name CLOSE_PAR
;
