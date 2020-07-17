#pragma once

#include "AtHtmlGrammar.h"

namespace At
{
	namespace Html
	{

		// Returns the contents of the quoted string, without the quotes. Does not perform conversions or resolve character references.
		Seq ReadQuotedString(ParseNode const& qsNode);

		// Returns true if the attribute node has a value, false if no value (e.g. just "disabled" or "checked").
		// If the value is quoted, returns the value without the quotes. Does not perform conversions or resolve character references.
		bool ReadAttrValue(ParseNode const& attrNode, Seq& value);

	}
}
