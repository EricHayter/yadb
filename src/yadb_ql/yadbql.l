%{
#include "yadbql_symbols.h"
%}
%option nounput yylineno
%option caseless

digit [0-9]
hex_digit [0-9A-F]

%%

";"                      { return SCOL; }
","                      { return COMMA; }
"("                      { return OPEN_PAR; }
")"                      { return CLOSE_PAR; }
"=="                     { return EQ; }



"\""(~"\""|"\"\"")*"\""   	{ return IDENTIFIER; }
"`"(~"`"|"``")*"`"         	{ return IDENTIFIER; }
"["~"]"*"]"                 { return IDENTIFIER; }

"\'"(~"\'"|"\'\'")*"\'"   	{ return STRING_LITERAL; }

(({digit})+("."{digit}*)?|("."{digit}+))("E"[-+]?{digit}+)? { return NUMERIC_LITERAL; }
"0x"{hex_digit}+            { return NUMERIC_LITERAL; }

[ \u00B\t\r\n]	

%%

int yywrap(void)
{
    return 1;
}

