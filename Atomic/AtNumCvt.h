#pragma once

#include "AtNum.h"


// These declarations are in a separate header so that AtEnc.h and AtEnsureFailDesc.h can include them

namespace At
{

	struct NumAlphabet
	{
		static char const* const Upper;
		static char const* const Lower;

		static inline char const* Get(CharCase cc) noexcept { return ((cc == CharCase::Upper) ? Upper : Lower); }
	};

	// Returns UINT_MAX if input is not a hex character in [0..9], [A..F], or [a..f]
	uint HexDigitValue(uint c) noexcept;

	struct AddSign { enum E { IfNeg, Always }; };

	// The buffer must have adequate room for output. Output is NOT null-terminated.
	sizet UInt2Buf(uint64 value, byte* buf, uint base = 10, uint zeroPadWidth = 0, CharCase charCase = CharCase::Upper, uint spacePadWidth = 0) noexcept;
	sizet SInt2Buf(int64 value, byte* buf, AddSign::E addSign = AddSign::IfNeg, uint base = 10, uint zeroPadWidth = 0, CharCase charCase = CharCase::Upper) noexcept;
	sizet Dbl2Buf(double value, byte* buf, uint prec = 4) noexcept;

}
