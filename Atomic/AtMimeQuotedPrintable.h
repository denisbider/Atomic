#pragma once

#include "AtStr.h"


namespace At
{
	namespace Mime
	{
		// The destination string is not cleared. Encoded data is appended to the string.

		void QuotedPrintableEncode(Seq plain, Enc& encoded);
	

		// The destination buffer is not cleared. Decoded data is appended.
		// Invalid escape sequences are preserved verbatim.

		void QuotedPrintableDecode(Seq& encoded, Enc& decoded);
	}
}
