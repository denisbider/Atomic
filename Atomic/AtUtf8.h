#pragma once

#include "AtStr.h"
#include "AtUtf8Lit.h"


namespace At
{
	namespace Utf8
	{
		enum { MaxBytesPerChar = 4 };

		// A UTF-8 character is encoded as one leading byte, followed by 1 - 3 trailing bytes
		inline bool IsLeadingByte (uint c) { return (c & 0xFFFFFFC0) == 0xC0; }
		inline bool IsTrailingByte(uint c) { return (c & 0xFFFFFFC0) == 0x80; }

		struct ReadResult { enum E { OK, NeedMore, CharStartNotFound, InvalidLeadingByte, InvalidTrailingByte, Overlong, InvalidSurrogate, InvalidCodePoint }; };
		ReadResult::E ReadCodePoint    (Seq& s, uint& v) noexcept;
		ReadResult::E RevReadCodePoint (Seq& s, uint& v) noexcept;

		sizet CodePointEncodedBytes (uint c);
		uint  WriteCodePoint        (byte* p, uint c);	// Returns number of bytes written

		bool IsValid(Seq s) noexcept;


		// Truncates a UTF-8 string at a character boundary
		Seq TruncateStr(Seq in, sizet maxBytes) noexcept;
	}

	// Cannot be defined in Seq because Seq is used in this header file
	inline uint Seq::ReadUtf8Char() noexcept { uint c; Utf8::ReadCodePoint(*this, c); return c; }

}
