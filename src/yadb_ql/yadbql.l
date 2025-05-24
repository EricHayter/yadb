%{
#include "yadbql_symbols.h"
%}
%option nounput yylineno
%option caseless

digit [0-9]
hex_digit [0-9A-F]

%%

";"		{ return SCOL; }
","    	{ return COMMA; }
"("    	{ return OPEN_PAR; }
")"    	{ return CLOSE_PAR; }
"=="   	{ return EQ; }

"CREATE"	{ return CREATE; }
"DELETE"	{ return DELETE; }
"INSERT"	{ return INSERT; }
"DROP"		{ return DROP; }
"WHERE"		{ return WHERE; }
"FROM"		{ return FROM; }
"INTO"		{ return INTO; }
"TABLE"		{ return TABLE; }
"VALUES"	{ return VALUES; }

"\""(~"\""|"\"\"")*"\"" |
"`"(~"`"|"``")*"`" |
"["~"]"*"]"                 { return IDENTIFIER; }

"\'"(~"\'"|"\'\'")*"\'"   	{ return STRING_LITERAL; }

({digit})+("."{digit}*)? |
("."{digit}+) |
"0x"{hex_digit}+            { return NUMERIC_LITERAL; }

[ \u00B\t\r\n]	

%%

int yywrap(void)
{
    return 1;
}

