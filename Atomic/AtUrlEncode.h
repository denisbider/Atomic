#pragma once

#include "AtNameValuePairs.h"

namespace At
{
	// Conversion to UTF-8 must occur after each URL decoding. It must NOT be done beforehand.
	// If code page conversion is done beforehand, parts of URL decoded values could be partially in UTF-8
	// (parts that were not percent-escaped) and partially in the input code page (parts that were percent-escaped).

	void ParseUrlEncodedNvp(InsensitiveNameValuePairsWithStore& nvp, Seq encoded, UINT inCp);
}
