#pragma once

#include "AtStr.h"

namespace At
{
	// Escapes occurrences of \, ", \r, and \n by prefixing a backslash (\)
	Str QuoteStr(Seq s);

	// Reads a string double-quoted using the rules of QuoteStr().
	// Ends reading immediately after the closing double quote character, or at end of string. 
	Str ReadQuotedStr(Seq& s);
}
