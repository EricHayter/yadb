// This is a basic attempt at making a stripped down SQL-like language for
// yadb. Design of the lanaguage is taken from the SQLite query langauge.
lexer grammar YadbQLLexer;

options {
    caseInsensitive = true;
}

// Basic operators
// TODO might need to strip this down depending on how ANTLR handles everything
SCOL      : ';';
DOT       : '.';
OPEN_PAR  : '(';
CLOSE_PAR : ')';
COMMA     : ',';
ASSIGN    : '=';
STAR      : '*';
PLUS      : '+';
MINUS     : '-';
TILDE     : '~';
PIPE2     : '||';
DIV       : '/';
MOD       : '%';
LT2       : '<<';
GT2       : '>>';
AMP       : '&';
PIPE      : '|';
LT        : '<';
LT_EQ     : '<=';
GT        : '>';
GT_EQ     : '>=';
EQ        : '==';
NOT_EQ1   : '!=';
NOT_EQ2   : '<>';

// KEYWORDS
CREATE_				: 'CREATE';
DELETE_				: 'DELTE';
INSERT_				: 'INSERT';
DROP_				: 'DROP';
WHERE_				: 'WHERE';
FROM_				: 'FROM';
INTO_				: 'INTO';
TABLE_				: 'TABLE';
VALUES_				: 'VALUES';

TRUE_				: 'TRUE';
FALSE_				: 'FALSE';
NULL_				: 'NULL';
CURRENT_TIME_		: 'CURRENT_TIME';
CURRENT_DATE_		: 'CURRENT_DATE';
CURRENT_TIMESTAMP_	: 'CURRENT_TIMESTAMP';

IDENTIFIER:
    '"' (~'"' | '""')* '"'
    | '`' (~'`' | '``')* '`'
    | '[' ~']'* ']'
    | [A-Z_\u007F-\uFFFF] [A-Z_0-9\u007F-\uFFFF]*
;

NUMERIC_LITERAL: ((DIGIT+ ('.' DIGIT*)?) | ('.' DIGIT+)) ('E' [-+]? DIGIT+)? | '0x' HEX_DIGIT+;

BIND_PARAMETER: '?' DIGIT* | [:@$] IDENTIFIER;

STRING_LITERAL: '\'' ( ~'\'' | '\'\'')* '\'';

SINGLE_LINE_COMMENT: '--' ~[\r\n]* (('\r'? '\n') | EOF) -> channel(HIDDEN);

MULTILINE_COMMENT: '/*' .*? '*/' -> channel(HIDDEN);

SPACES: [ \u000B\t\r\n] -> channel(HIDDEN);

UNEXPECTED_CHAR: .;

fragment HEX_DIGIT : [0-9A-F];
fragment DIGIT     : [0-9];
