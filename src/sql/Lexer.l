%{
#include <string>
#include <iostream>
#include "Parser.h"

// yylval will be passed as a parameter
#define YY_DECL int yylex(yy::parser::semantic_type* yylval)
%}

%option noyywrap
%option case-insensitive
%option noinput
%option nounput

%%

[ \t\n]+                { /* skip whitespace */ }

"SELECT"                { return yy::parser::token::SELECT; }
"FROM"                  { return yy::parser::token::FROM; }
"WHERE"                 { return yy::parser::token::WHERE; }
"INSERT"                { return yy::parser::token::INSERT; }
"INTO"                  { return yy::parser::token::INTO; }
"VALUES"                { return yy::parser::token::VALUES; }
"CREATE"                { return yy::parser::token::CREATE; }
"TABLE"                 { return yy::parser::token::TABLE; }
"DROP"                  { return yy::parser::token::DROP; }
"DELETE"                { return yy::parser::token::DELETE; }
"UPDATE"                { return yy::parser::token::UPDATE; }
"SET"                   { return yy::parser::token::SET; }
"AND"                   { return yy::parser::token::AND; }
"OR"                    { return yy::parser::token::OR; }

"*"                     { return yy::parser::token::STAR; }
"("                     { return yy::parser::token::LPAREN; }
")"                     { return yy::parser::token::RPAREN; }
","                     { return yy::parser::token::COMMA; }
";"                     { return yy::parser::token::SEMICOLON; }
"="                     { return yy::parser::token::EQ; }
"!="                    { return yy::parser::token::NEQ; }
"<"                     { return yy::parser::token::LT; }
">"                     { return yy::parser::token::GT; }
"<="                    { return yy::parser::token::LE; }
">="                    { return yy::parser::token::GE; }

[a-zA-Z_][a-zA-Z0-9_]*  {
    yylval->build<std::string>(yytext);
    return yy::parser::token::IDENTIFIER;
}
-?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)? {
    yylval->build<std::string>(yytext);
    return yy::parser::token::NUMBER;
}
'[^']*'                 {
    // strip the surrounding quotes
    yylval->build<std::string>(std::string(yytext + 1, static_cast<std::size_t>(yyleng) - 2));
    return yy::parser::token::STRING;
}

.                       { std::cerr << "Unexpected character: " << yytext << std::endl; }

%%
