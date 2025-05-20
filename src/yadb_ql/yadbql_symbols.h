#pragma once

// List of symbols to be used by the flex lexical analyzer. The language/
// keywords should be SQL-like but at the moment this is VERY stripped down
// to provide a simple proof of concept.
typedef enum YadbQLSymbol
{
	/* Basic symbols */
	SCOL = 1, 		// ;
	COMMA,			// ,
	OPEN_PAR, 		// (
	CLOSE_PAR, 		// )
	EQ,				// ==

	/* Keywords */
	CREATE,
	DELETE,
	INSERT,
	DROP,
	WHERE,
	FROM,
	INTO,
	TABLE,
	VALUES,

	/* literal values */
	IDENTIFIER,
	STRING_LITERAL,
	NUMERIC_LITERAL,


} YadbQLSymbol;
