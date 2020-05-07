#include "AtIncludes.h"
#include "AtSeq.h"

#include "AtNumCvt.h"
#include "AtTime.h"
#include "AtUnicode.h"
#include "AtUtf8.h"


namespace At
{

	// Seq

	char const* const Seq::AsciiWsBytes = " \t\n\r\f";
	char const* const Seq::HexDecodeSkipBytes = ": \t\n\r\f";


	sizet Seq::CalcWidth(sizet precedingWidth, sizet tabStop) const noexcept
	{
		Seq reader { *this };
		sizet width {};
		uint c;
		while ((c = reader.ReadUtf8Char()) != UINT_MAX)
			if (c == 9)
				width = ((((precedingWidth + width) / tabStop) + 1) * tabStop) - precedingWidth;
			else
				width += Unicode::CharInfo::Get(c).m_width;
		return width;
	}


	sizet Seq::GeneralPurposeHexDecode(byte* decoded, sizet maxBytes, char const* skipInput) noexcept
	{
		sizet nrBytesDecoded = 0;
		while (nrBytesDecoded != maxBytes)
		{
			DropToFirstByteNotOf(skipInput);
			if (!n)
				return nrBytesDecoded;

			uint b = ReadHexEncodedByte();
			if (b == UINT_MAX)
				return nrBytesDecoded;

			*decoded++ = (byte) b;
			++nrBytesDecoded;
		}
		return nrBytesDecoded;
	}


	sizet Seq::ReadHexDecodeInto(Enc& enc, sizet maxBytesToDecode, char const* skipInput)
	{
		maxBytesToDecode = PickMin<sizet>(maxBytesToDecode, n / 2);
		sizet origLen = enc.Len();
		enc.ResizeInc(maxBytesToDecode);
		sizet bytesDecoded = GeneralPurposeHexDecode(enc.Ptr() + origLen, maxBytesToDecode, skipInput);
		sizet newLen = origLen + bytesDecoded;
		EnsureAbort(newLen <= enc.Len());
		enc.Resize(newLen);
		return bytesDecoded;
	}


	uint Seq::FirstByte() noexcept
	{
		if (!n)
			return UINT_MAX;	

		return *p;
	}


	uint Seq::LastByte() noexcept
	{
		if (!n)
			return UINT_MAX;

		return p[n-1];
	}


	uint Seq::ReadByte() noexcept
	{
		if (!n)
			return UINT_MAX;	

		--n;
		return *p++;
	}


	uint Seq::ReadHexEncodedByte() noexcept
	{
		if (n < 2)
			return UINT_MAX;

		uint d1 { HexDigitValue(p[0]) };
		if (d1 == UINT_MAX)
			return UINT_MAX;
		
		uint d2 { HexDigitValue(p[1]) };
		if (d2 == UINT_MAX)
			return UINT_MAX;

		n -= 2;
		p += 2;

		return (d1 << 4) | d2;
	}


	Seq Seq::ReadBytes(sizet m) noexcept
	{
		if (m > n)
			m = n;

		Seq ret { p, m };
		p += m;
		n -= m;
		return ret;
	}


	bool Seq::ReadBytesInto(void* pRead, sizet m) noexcept
	{
		if (m > n)
			return false;

		Mem::Copy((byte*) pRead, p, m);
		p += m;
		n -= m;
		return true;
	}


	Seq Seq::ReadUtf8_MaxBytes(sizet maxBytes) noexcept
	{
		Seq ret { Utf8::TruncateStr(*this, maxBytes) };
		EnsureAbort(n >= ret.n);
		p += ret.n;
		n -= ret.n;
		return ret;
	}


	Seq Seq::ReadUtf8_MaxChars(sizet maxChars) noexcept
	{
		Seq   reader        { *this };
		sizet nrCharsToRead { maxChars };

		while (nrCharsToRead)
		{
			uint c;
			Utf8::ReadResult::E readResult = Utf8::ReadCodePoint(reader, c);
			if (readResult != Utf8::ReadResult::OK)
				break;
			--nrCharsToRead;
		}

		Seq result = Seq(p, n - reader.n);
		DropBytes(result.n);
		return result;
	}


	Seq Seq::ReadToByte(uint b, sizet m) noexcept
	{
		Seq orig { p, n };

		if (b > UINT8_MAX)
		{
			p += n;
			n = 0;
			goto Done;
		}

		while (n)
		{
			if (m != SIZE_MAX && !m--)
				goto Done;
			if (*p == b)
				goto Done;
			++p;
			--n;
		}

	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToFirstByteOf(char const* bytes, sizet m) noexcept
	{
		Seq orig { p, n };
		while (n)
		{
			if (m != SIZE_MAX && !m--)
				goto Done;
			if (ZChr(bytes, *p))
				goto Done;
			++p;
			--n;
		}
	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToFirstByteNotOf(char const* bytes, sizet m) noexcept
	{
		Seq orig { p, n };
		while (n)
		{
			if (m != SIZE_MAX && !m--)
				goto Done;
			if (!ZChr(bytes, *p))
				goto Done;
			++p;
			--n;
		}
	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToFirstByteOfType(CharCriterion criterion, sizet m) noexcept
	{
		Seq orig { p, n };
		while (n)
		{
			if (m != SIZE_MAX && !m--)
				goto Done;
			if (criterion(*p))
				goto Done;
			++p;
			--n;
		}
	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToFirstByteNotOfType(CharCriterion criterion, sizet m) noexcept
	{
		Seq orig { p, n };
		while (n)
		{
			if (m != SIZE_MAX && !m--)
				goto Done;
			if (!criterion(*p))
				goto Done;
			++p;
			--n;
		}
	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToFirstUtf8CharOfType(CharCriterion criterion) noexcept
	{
		Seq orig { p, n };
		Seq reader = *this;
		while (n)
		{
			uint c;
			Utf8::ReadResult::E readResult = Utf8::ReadCodePoint(reader, c);
			if (readResult != Utf8::ReadResult::OK)
				reader.DropByte();
			else if (criterion(c))
				goto Done;

			*this = reader;
		}
	Done:
		orig.n -= n;
		return orig;
	}
	
	
	Seq Seq::ReadToFirstUtf8CharNotOfType(CharCriterion criterion) noexcept
	{
		Seq orig { p, n };
		Seq reader = *this;
		while (n)
		{
			uint c;
			Utf8::ReadResult::E readResult = Utf8::ReadCodePoint(reader, c);
			if (readResult != Utf8::ReadResult::OK)
				reader.DropByte();
			else if (!criterion(c))
				goto Done;

			*this = reader;
		}
	Done:
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadToString(Seq str, CaseMatch cm) noexcept
	{
		Seq orig(p, n);
		while (true)
		{
			if (n < str.n)
			{
				p += n;
				n = 0;
				break;
			}
			if (Seq(p, str.n).Compare(str, cm) == 0)
				break;
			++p;
			--n;
		}
		orig.n -= n;
		return orig;
	}


	Seq Seq::ReadLeadingNewLine() noexcept
	{
		if (!n)
			return Seq(p, 0);

		byte other;
		if (p[0] == 13)
			other = 10;
		else if (p[0] == 10)
			other = 13;
		else
			return Seq(p, 0);

		sizet skip = 1;
		if (n > 1 && p[1] == other)
			++skip;

		return ReadBytes(skip);
	}


	uint64 Seq::ReadNrUInt64(byte base, uint64 max) noexcept
	{
		EnsureAbortWithNr(base >= 2 && base <= 36, base);

		uint64 v = 0;
		while (n)
		{
			uint digit;
			if (*p >= '0' && *p <= '9')
				digit = (uint) (*p - '0');
			else if (*p >= 'A' && *p <= 'Z')
				digit = (uint) ((*p - 'A') + 10);
			else if (*p >= 'a' && *p <= 'z')
				digit = (uint) ((*p - 'a') + 10);
			else
				break;
		
			if (digit >= base)
				break;

			++p;
			--n;

			if (digit >= max)
				return max;
		
			uint64 limit = (max - digit) / base;
			if (v > limit)
				return max;

			v = v*base + digit;
		}
	
		return v;
	}


	int64 Seq::ReadNrSInt64(byte base, int64 min, int64 max) noexcept
	{
		bool neg = false;
		if (n && p[0] == '-')
		{
			neg = true;
			DropByte();
		}
	
		uint64 v = ReadNrUInt64(base);
		int64 sv;
		if (neg)
		{
			uint64 negMax = ((uint64) INT64_MAX) + 1;
			if (v >= negMax)
				sv = INT64_MIN;
			else
				sv = -((int64) v);
		}
		else
		{
			if (v > INT64_MAX)
				sv = INT64_MAX;
			else
				sv = (int64) v;
		}

		if (sv < min)
			sv = min;
		else if (sv > max)
			sv = max;
	
		return sv;
	}


	double Seq::ReadDouble() noexcept
	{
		double sign = 1.0;
		if (n && *p == '-')
		{
			sign = -1.0;
			DropByte();
		}
	
		uint64 integerPart = ReadNrUInt64Dec();
		if (integerPart == UINT64_MAX)
			return std::numeric_limits<double>::quiet_NaN();

		if (!n || *p != '.')
			return sign * ((double) integerPart);

		DropByte();
	
		uint64 decimalPart = ReadNrUInt64Dec(9999999999999999999ULL);
		if (decimalPart == UINT64_MAX)
			return std::numeric_limits<double>::quiet_NaN();

		uint64 denominator = 10;
		while (denominator <= decimalPart)
			denominator *= 10;
	
		return (sign * ((double) integerPart)) + ((double) decimalPart) / ((double) denominator);
	}


	Time Seq::ReadIsoStyleTimeStr() noexcept
	{
		return Time().ReadIsoStyleTimeStr(*this);
	}


	void Seq::ForEachNonEmptyToken(char const* separatorBytes, std::function<bool(Seq)> onToken) const
	{
		Seq reader = *this;
		while (reader.n)
		{
			reader.DropToFirstByteNotOf(separatorBytes);
			Seq token = reader.ReadToFirstByteOf(separatorBytes);
			if (token.n)
				if (!onToken(token))
					break;
		}
	}


	uint Seq::ReadLastUtf8Char() noexcept
	{
		uint c;
		Utf8::ReadResult::E readResult = Utf8::ReadLastCodePoint(*this, c);
		if (readResult == Utf8::ReadResult::OK)
			return c;

		return UINT_MAX;
	}


	int Seq::Compare(Seq x, CaseMatch cm) const noexcept
	{
		if (n > x.n) return -x.Compare(*this, cm);
		int diff { (cm == CaseMatch::Exact) ? memcmp(p, x.p, n) : MemCmpInsensitive(p, x.p, n) };
		if (diff) return diff;
		if (n == x.n) return 0;
		return -1;
	}

	bool Seq::ConstTimeEqualExact(Seq x) const noexcept
	{
		if (n != x.n) return false;		// We don't try to make it constant-time in this case
		sizet diff {};
		for (sizet i=0; i!=n; ++i)
			diff |= (p[i] ^ x.p[i]);
		return diff == 0;
	}


	Seq Seq::CommonPrefixExact(Seq x) const noexcept
	{
		sizet i;
		for (i=0; i!=n && i!=x.n; ++i)
			if (p[i] != x.p[i])
				break;
		return Seq(p, i);
	}

	Seq Seq::CommonPrefixInsensitive(Seq x) const noexcept
	{
		sizet i;
		for (i=0; i!=n && i!=x.n; ++i)
			if (ToLower(p[i]) != ToLower(x.p[i]))
				break;
		return Seq(p, i);
	}


	Seq Seq::ReadToAfterLastByte(uint b) noexcept
	{
		if (b > UINT8_MAX)
			return Seq { p, 0 };

		byte const* r { p + n };
		while (r != p)
		{
			--r;
			if (*r == b)
			{
				++r;
				break;
			}
		}

		return ReadBytes((sizet) (r - p));
	}

	Seq Seq::ReadToAfterLastByteOf(char const* bytes) noexcept
	{
		byte const* r { p + n };
		while (r != p)
		{
			--r;
			if (ZChr(bytes, *r))
			{
				++r;
				goto Done;
			}
		}

	Done:
		return ReadBytes((sizet) (r - p));
	}

	Seq Seq::ReadToAfterLastByteNotOf(char const* bytes) noexcept
	{
		byte const* r { p + n };
		while (r != p)
		{
			--r;
			if (!ZChr(bytes, *r))
			{
				++r;
				goto Done;
			}
		}

	Done:
		return ReadBytes((sizet) (r - p));
	}


	Seq Seq::ReadToAfterLastUtf8CharOfType(CharCriterion criterion) noexcept
	{
		Seq reader = *this;
		while (reader.n)
		{
			uint c;
			Utf8::ReadResult::E readResult = Utf8::ReadLastCodePoint(reader, c);
			if (readResult != Utf8::ReadResult::OK || criterion(c))
				break;

			*this = reader;
		}

		return *this;
	}
	
	
	Seq Seq::ReadToAfterLastUtf8CharNotOfType(CharCriterion criterion) noexcept
	{
		Seq reader = *this;
		while (reader.n)
		{
			uint c;
			Utf8::ReadResult::E readResult = Utf8::ReadLastCodePoint(reader, c);
			if (readResult != Utf8::ReadResult::OK || !criterion(c))
				break;

			*this = reader;
		}

		return *this;
	}


	void Seq::ForEachLine(std::function<bool (Seq)> onLine) const
	{
		Seq reader = *this;
		while (reader.n)
		{
			Seq line = reader.ReadToByte(10);
			reader.DropByte();

			if (line.EndsWithByte(13))
				--line.n;
			if (!onLine(line))
				break;
		}
	}


	template <typename SeqOrStr>
	Vec<SeqOrStr> Seq::SplitLines(uint splitFlags)
	{
		Vec<SeqOrStr> lines;
		ForEachLine([splitFlags, &lines] (Seq line) -> bool
			{
				if (0 != (splitFlags & SplitFlags::TrimLeft  )) line = line.TrimLeft();
				if (0 != (splitFlags & SplitFlags::TrimRight )) line = line.TrimLeft();
				if (!(splitFlags & SplitFlags::DiscardEmpty) || line.n)
					lines.Add(line);
				return true;
			} );
		return lines;
	}

	template Vec<Seq> Seq::SplitLines<Seq>(uint);
	template Vec<Str> Seq::SplitLines<Str>(uint);


	int Seq::MemCmpInsensitive(byte const* a, byte const* b, sizet n) noexcept
	{
		while (n--)
		{
			int diff { ((int) ToLower(*a++)) - ((int) ToLower(*b++)) };
			if (diff) return diff;
		}

		return 0;
	}

}
