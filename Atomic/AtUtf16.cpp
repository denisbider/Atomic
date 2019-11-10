#include "AtIncludes.h"
#include "AtUtf16.h"

#include "AtUnicode.h"

namespace At
{
	namespace Utf16
	{

		template <wchar_t WcharTransform(wchar_t)>
		uint WriteCodePoint(wchar_t* p, uint c)
		{
			EnsureThrow(Unicode::IsValidCodePoint(c));

			if (c <= 0xFFFF)
			{
				*p++ = WcharTransform((wchar_t) c);
				return 1;
			}
			else
			{
				uint v = c - 0x10000;
				*p++ = WcharTransform(0xD800 + ((v >> 10) & 0x3FF));
				*p++ = WcharTransform(0xDC00 + ( v        & 0x3FF));
				return 2;
			}
		}

		template uint WriteCodePoint<Identity>(wchar_t*, uint);
		template uint WriteCodePoint<ByteSwap>(wchar_t*, uint);



		template <wchar_t WcharTransform(wchar_t)>
		void AddCodePoint(Vec<wchar_t>& v, uint c)
		{
			sizet origLen = v.Len();
			v.ResizeInc(2);
			uint n = WriteCodePoint<WcharTransform>(v.Ptr() + origLen, c);
			v.Resize(origLen + n);
		}

		template void AddCodePoint<Identity>(Vec<wchar_t>&, uint);
		template void AddCodePoint<ByteSwap>(Vec<wchar_t>&, uint);


	}
}
