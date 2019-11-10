#pragma once

#include "AtStr.h"

namespace At
{
	namespace Utf16
	{

		inline wchar_t Identity(wchar_t c) { return c; }
		inline wchar_t ByteSwap(wchar_t c) { return _byteswap_ushort(c); }

		template <wchar_t WcharTransform(wchar_t) = Identity>
		uint WriteCodePoint(wchar_t* p, uint c);		// Returns number of wchar_t written
		
		template <wchar_t WcharTransform(wchar_t) = Identity>
		void AddCodePoint(Vec<wchar_t>& v, uint c);

	}
}

