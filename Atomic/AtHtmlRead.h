#pragma once

#include "AtHtmlGrammar.h"

namespace At
{
	namespace Html
	{

		Seq ReadQuotedString(ParseNode const& qsNode, PinStore& store);

		// Returns true if the attribute node has a value, false if no value (e.g. just "disabled" or "checked").
		bool ReadAttrValue(ParseNode const& attrNode, PinStore& store, Seq& value);

	}
}
