%{
#include <string.h>
#include <stdlib.h>
#include "Parser.h"
%}

%option noyywrap
%option case-insensitive

%%

[ \t\n]+                { /* skip whitespace */ }

"SELECT"                { return SELECT; }
"FROM"                  { return FROM; }
"WHERE"                 { return WHERE; }
"INSERT"                { return INSERT; }
"INTO"                  { return INTO; }
"VALUES"                { return VALUES; }
"CREATE"                { return CREATE; }
"TABLE"                 { return TABLE; }
"DROP"                  { return DROP; }
"DELETE"                { return DELETE; }
"UPDATE"                { return UPDATE; }
"SET"                   { return SET; }
"AND"                   { return AND; }
"OR"                    { return OR; }
"INTEGER"               { return INTEGER; }
"TEXT"                  { return TEXT; }

"*"                     { return STAR; }
"("                     { return LPAREN; }
")"                     { return RPAREN; }
","                     { return COMMA; }
";"                     { return SEMICOLON; }
"="                     { return EQ; }
"!="                    { return NEQ; }
"<"                     { return LT; }
">"                     { return GT; }
"<="                    { return LE; }
">="                    { return GE; }

[a-zA-Z_][a-zA-Z0-9_]*  {
    yylval.sval = strdup(yytext);
    return IDENTIFIER;
}
[0-9]+                  {
    yylval.ival = atoi(yytext);
    return NUMBER;
}
'[^']*'                 {
    yylval.sval = strdup(yytext);
    return STRING;
}

.                       { fprintf(stderr, "Unexpected character: %s\n", yytext); }

%%
