#pragma once

#include "AtException.h"
#include "AtStr.h"
#include "AtVec.h"


namespace At
{

	struct NullTerm { enum E { No, Yes }; };

	void ToUtf16(Seq in, Vec<wchar_t>& out, UINT inCodePage, NullTerm::E nt=NullTerm::No);

	// nWideChars may be -1 to indicate pStr is null-terminated. In this case, this function will omit the null-terminator from the output.
	void NormalizeUtf16(PCWSTR pStr, int nWideChars, Vec<wchar_t>& out);

	// nWideChars may be -1 to indicate pStr is null-terminated. In this case, this function will omit the null-terminator from the output.
	void FromUtf16(PCWSTR pStr, int nWideChars, Enc& enc, UINT outCodePage);


	// Converts a string in one code page to another code page.
	// Throws InputErr if the string cannot be converted due to its contents.
	// The convertBuf parameter allows the user to provide a reusable conversion buffer object,
	// helping avoid many memory allocations when many strings are being converted.

	void StrCvtCp(Seq in, Str& out, UINT inCodePage, UINT outCodePage, Vec<wchar_t>& convertBuf);

	inline void StrCvtCp(Seq in, Str& out, UINT inCodePage, UINT outCodePage)
		{ Vec<wchar_t> convertBuf; StrCvtCp(in, out, inCodePage, outCodePage, convertBuf); }


	// Use ToNormUtf8 instead of StrCvtCp when accepting input from external sources and converting it for internal use

	void ToNormUtf8(Seq in, Str& out, UINT inCodePage, Vec<wchar_t>& convertBuf1, Vec<wchar_t>& convertBuf2);

	inline void ToNormUtf8(Seq in, Str& out, UINT inCodePage)
		{ Vec<wchar_t> convertBuf1, convertBuf2; ToNormUtf8(in, out, inCodePage, convertBuf1, convertBuf2); }


	// For case insensitive comparisons and sorting, consider supporting LCMapStringEx(LCMAP_SORTKEY)
}
