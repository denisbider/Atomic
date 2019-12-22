#pragma once

#include "AtStr.h"


namespace At
{
	namespace Mime
	{
		// Worst case: every byte requires a "=HH" hex encoding and a 3-byte newline ("=\r\n") has to be inserted every 75 characters
		// For faster computation, we use "plainLen >> 6" as an overestimate of "plainLen / 75"
		inline sizet QuotedPrintableEncode_MaxLen(sizet plainLen) { return 3*(plainLen + (plainLen >> 6)); }

		Seq QuotedPrintableEncode(Seq plain, Enc& encoded);
	
		inline sizet QuotedPrintableDecode_MaxLen(sizet encodedLen) { return encodedLen; }

		// Invalid escape sequences are preserved verbatim
		Seq QuotedPrintableDecode(Seq& encoded, Enc& decoded);
	}
}
