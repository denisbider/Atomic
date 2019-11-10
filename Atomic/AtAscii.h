#pragma once

#include "AtNum.h"

namespace At
{
	// Use these functions if you don't want the current locale setting
	// to influence the behavior of the program.

	typedef bool (*CharCriterion)(uint c);

	namespace Ascii
	{
		inline bool IsNull            (uint c) { return c == 0; }
		inline bool IsWhitespace      (uint c) { return (c==9 || c==10 || c==13 || c==32); }
		inline bool IsHorizWhitespace (uint c) { return (c==9 || c==32); }
		inline bool IsVertWhitespace  (uint c) { return (c==10 || c==13); }
		inline bool IsDecDigit        (uint c) { return (c>='0' && c<='9'); }
		inline bool IsHexDigit        (uint c) { return IsDecDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
		inline bool IsAlphaUpper      (uint c) { return (c>='A' && c<='Z'); }
		inline bool IsAlphaLower      (uint c) { return (c>='a' && c<='z'); }
		inline bool IsAlpha           (uint c) { return IsAlphaUpper(c) || IsAlphaLower(c); }
		inline bool IsAlphaNum        (uint c) { return IsDecDigit(c) || IsAlpha(c); }
		inline bool IsControl         (uint c) { return c<=31 || c==127; }
		inline bool IsControlNonSpace (uint c) { return IsControl(c) && !IsWhitespace(c); }
		inline bool Is7Bit            (uint c) { return c<=127; }
		inline bool IsPrintable       (uint c) { return Is7Bit(c) && !IsControlNonSpace(c); }
		inline bool IsPunct           (uint c) { return IsPrintable(c) && !IsAlphaNum(c); }
		inline bool IsDoubleQuote     (uint c) { return c == '"'; }
	}
}
