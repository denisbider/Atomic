#include "AtIncludes.h"
#include "AtUtf8.h"

#include "AtUnicode.h"


namespace At
{
	namespace Utf8
	{
		namespace Lit
		{
			Seq const BOM      { "\xEF\xBB\xBF", 3 };	// U+FEFF
			Seq const Ellipsis { "\xE2\x80\xA6", 3 };	// U+2026
		}


		ReadResult::E ReadCodePoint(Seq& s, uint& v) noexcept
		{
			v = UINT_MAX;

			Seq reader { s };
			uint c { reader.ReadByte() };
			if (c == UINT_MAX)
				return ReadResult::NeedMore;

			uint n = 1;
			if (c >= 0x80)
			{
					 if (c >= 0xC0 && c < 0xE0) { n = 2; c &= 0x1F; }
				else if (c >= 0xE0 && c < 0xF0) { n = 3; c &= 0x0F; }
				else if (c >= 0xF0 && c < 0xF8) { n = 4; c &= 0x07; }
				else
					return ReadResult::InvalidLeadingByte;

				uint i { n };
				while (--i != 0)
				{
					uint t { reader.ReadByte() };
					if (t == UINT_MAX)
						return ReadResult::NeedMore;
					if ((t & 0xC0) != 0x80)
						return ReadResult::InvalidTrailingByte;
					
					c = (c << 6) | (t & 0x3F);
				}

				if ((n == 2 && c < 0x00080) ||
					(n == 3 && c < 0x00800) ||
					(n == 4 && c < 0x10000))
					return ReadResult::Overlong;
				if (Unicode::IsSurrogate((uint) c))
					return ReadResult::InvalidSurrogate;
				if (c > Unicode::MaxPossibleCodePoint)
					return ReadResult::InvalidCodePoint;
			}

			v = c;
			s.DropBytes(n);
			return ReadResult::OK;
		}


		ReadResult::E RevReadCodePoint(Seq& s, uint& v) noexcept
		{
			v = UINT_MAX;

			Seq reverseReader { s };
			sizet nrBytesRead {};
			while (true)
			{
				uint c { reverseReader.RevReadByte() };
				if (c == UINT_MAX)
					return ReadResult::CharStartNotFound;

				++nrBytesRead;
				if (c < 0x80)
				{
					if (nrBytesRead == 1)
					{
						v = (uint) c;
						s = reverseReader;
						return ReadResult::OK;
					}

					return ReadResult::CharStartNotFound;
				}

				if (IsLeadingByte(c))
					break;
				if (nrBytesRead == MaxBytesPerChar)
					return ReadResult::CharStartNotFound;
			}

			Seq tailReader { reverseReader.p + reverseReader.n, nrBytesRead };
			ReadResult::E readResult { ReadCodePoint(tailReader, v) };
			if (readResult != ReadResult::OK)
				return readResult;

			s = reverseReader;
			return ReadResult::OK;
		}


		sizet CodePointEncodedBytes(uint c)
		{
			if (c <=    0x7F) return 1;
			if (c >= 0x10000) return 4;
			if (c >= 0x00800) return 3;
			return 2;
		}


		uint WriteCodePoint(byte* p, uint c)
		{
			uint n;
			EnsureThrow(Unicode::IsValidCodePoint(c));

			if (c <= 0x7F)
			{
				n = 1;
				*p = (byte) c;
			}
			else
			{
					 if (c >= 0x10000) { n = 4; *p++ = (byte) (0xF0 | ((c >> 18) & 0x07)); }
				else if (c >= 0x00800) { n = 3; *p++ = (byte) (0xE0 | ((c >> 12) & 0x0F)); }
				else                   { n = 2; *p++ = (byte) (0xC0 | ((c >>  6) & 0x1F)); }

				for (uint i=(n-1); i!=0; )
				{
					--i;
					*p++ = (byte) (0x80 | ((c >> (6*i)) & 0x3F));
				}
			}

			return n;
		}


		bool IsValid(Seq s) noexcept
		{
			uint v;
			while (s.n != 0)
				if (ReadCodePoint(s, v) != ReadResult::OK)
					return false;

			return true;
		}


		Seq TruncateStr(Seq in, sizet maxBytes) noexcept
		{
			if (in.n <= maxBytes)
				return in;
	
			sizet newLen = maxBytes;
			while (newLen && IsTrailingByte(in.p[newLen]))
				--newLen;
	
			return Seq(in.p, newLen);
		}
		
	}
}
