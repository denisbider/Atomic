#include "AtIncludes.h"
#include "AtEncode.h"

namespace At
{
	// Atomic's variable-length encoding for up to 64-bit signed and unsigned integers
	//
	// Encoding has the following properties:
	// - Uses a single byte for values -40 to 199 inclusive, adds one extra length byte otherwise.
	// - Encoded values preserve their numerical sort order according to binary string sorting rules.
	// - Signed and unsigned values are encoded identically within the overlapping range (0 to 0x7FFFFFFFFFFFFFFF).
	// - Neatly encodes values 0 - 9 as ASCII characters '0' - '9' as a side effect of encoding rules.
	// - 8-byte negative values with most significant bit 0 are out of representable range and prohibited.
	// - Non-canonical encodings with superfluous bytes are prohibited.
	//
	// Interpretation according to first byte:
	//   0 = negative 8-byte value in two's complement follows, most significant bit must be 1, first byte must not be 0xFF, most significant byte first 
	//   1 = negative 7-byte value in two's complement follows, OR-ed with 0xFF00000000000000, first byte must not be 0xFF, most significant byte first
	//   2 = negative 6-byte value in two's complement follows, OR-ed with 0xFFFF000000000000, first byte must not be 0xFF, most significant byte first
	//   3 = negative 5-byte value in two's complement follows, OR-ed with 0xFFFFFF0000000000, first byte must not be 0xFF, most significant byte first
	//   4 = negative 4-byte value in two's complement follows, OR-ed with 0xFFFFFFFF00000000, first byte must not be 0xFF, most significant byte first
	//   5 = negative 3-byte value in two's complement follows, OR-ed with 0xFFFFFFFFFF000000, first byte must not be 0xFF, most significant byte first
	//   6 = negative 2-byte value in two's complement follows, OR-ed with 0xFFFFFFFFFFFF0000, first byte must not be 0xFF, most significant byte first
	//   7 = negative 1-byte value in two's complement follows, OR-ed with 0xFFFFFFFFFFFFFF00, the unsigned byte must be < 216 (i.e. the final signed value must be < -40)
	//   8 = the value -40
	//   9 = the value -39
	//    ...
	//  46 = the value -2
	//  47 = the value -1
	//  48 = the value 0 (equals ASCII '0')
	//  49 = the value 1 (equals ASCII '1')
	//    ...
	// 246 = the value 198
	// 247 = the value 199
	// 248 = positive unsigned 1-byte value follows, the byte must be > 199
	// 249 = positive unsigned 2-byte value follows, first byte must not be zero, most significant byte first
	// 250 = positive unsigned 3-byte value follows, first byte must not be zero, most significant byte first
	// 251 = positive unsigned 4-byte value follows, first byte must not be zero, most significant byte first
	// 252 = positive unsigned 5-byte value follows, first byte must not be zero, most significant byte first
	// 253 = positive unsigned 6-byte value follows, first byte must not be zero, most significant byte first
	// 254 = positive unsigned 7-byte value follows, first byte must not be zero, most significant byte first
	// 255 = positive unsigned 8-byte value follows, first byte must not be zero, most significant byte first



	// VarUInt64

	sizet EncodeVarUInt64_Size(uint64 n)
	{
		sizet size = 1;

		if (n > 199)
		{
			do
			{
				++size;
				n >>= 8;
			}
			while (n != 0);
		}

		return size;
	}


	void EncodeVarUInt64(Enc& enc, uint64 n)
	{
		sizet      encodedSize = EncodeVarUInt64_Size(n);
		Enc::Write write       = enc.IncWrite(encodedSize);
		byte*      p           = write.Ptr();

		if (encodedSize == 1)
			*p++ = (byte) (48 + n);
		else
		{
			sizet nrBytes = encodedSize - 1;
			*p++ = (byte) (247U + nrBytes);

			while (nrBytes != 0)
			{
				--nrBytes;
				*p++ = ((n >> (nrBytes * 8)) & 0xFF);
			}
		}

		write.AddUpTo(p);
		EnsureThrow(write.Met());
	}


	bool DecodeVarUInt64(Seq& s, uint64& n)
	{
		uint c { s.ReadByte() };
		if (c < 48 || c == UINT_MAX)
			return false;

		if (c <= 247)
			n = (uint64) (c - 48);
		else
		{
			sizet len { (sizet) (c - 247) };
			n = 0;

			bool isLeading = true;
			do
			{
				c = s.ReadByte();
				if (c == UINT_MAX)
					return false;				// Premature end of data

				if (isLeading)
				{
					if (len == 1)
					{
						if (c <= 199)
							return false;		// Non-canonical - values from 0 to 199 inclusive must be encoded with a single byte
					}
					else
					{
						if (c == 0)
							return false;		// Non-canonical - superfluous leading zero byte(s)
					}

					isLeading = false;
				}

				n = (n << 8) | (byte) c;
			}
			while (--len);
		}
	
		return true;
	}



	// VarSInt64

	sizet EncodeVarSInt64_Size(int64 n)
	{
		if (n >= 0)
			return EncodeVarUInt64_Size((uint64) n);

		if (n >= -40)
			return 1;

		uint64 u     { (uint64) n };
		sizet  size  { 9 };
		uint64 msbFF { 0xFF00000000000000ULL };
		while ((u & msbFF) == msbFF)
		{
			--size;
			u <<= 8;
		}

		return size;
	}


	void EncodeVarSInt64(Enc& enc, int64 n)
	{
		sizet      encodedSize = EncodeVarSInt64_Size(n);
		Enc::Write write       = enc.IncWrite(encodedSize);
		byte*      p           = write.Ptr();

		if (encodedSize == 1)
			*p++ = (byte) (48 + n);
		else
		{
			sizet nrBytes { encodedSize - 1 };
			if (n >= 0)
				*p++ = (byte) (247 + nrBytes);
			else
				*p++ = (byte) (8 - nrBytes);

			uint64 u { (uint64) n };
			while (nrBytes != 0)
			{
				--nrBytes;
				*p++ = ((u >> (nrBytes * 8)) & 0xFF);
			}
		}

		write.AddUpTo(p);
		EnsureThrow(write.Met());
	}


	bool CanTryDecodeVarSInt64(Seq s)
	{
		uint c { s.ReadByte() };
		if (c == UINT_MAX) return false;
		if (c >  247)      return s.n >= (sizet) (c - 247);
		if (c <  8)        return s.n >= (sizet) (8 - c);
		return true;
	}


	bool DecodeVarSInt64(Seq& s, int64& n)
	{
		uint c { s.ReadByte() };
		if (c == UINT_MAX)
			return false;

		if (c >= 8 && c <= 247)
			n = c - 48;
		else
		{
			sizet len;
			bool isNegative;

			if (c > 247)
			{
				n = 0;
				len = (sizet) (c - 247);
				isNegative = false;
			}
			else
			{
				n = (int64) UINT64_MAX;
				len = (sizet) (8 - c);
				isNegative = true;
			}

			bool isLeading = true;
			do
			{
				c = s.ReadByte();
				if (c == UINT_MAX)
					return false;					// Premature end of data

				if (isLeading)
				{
					if (isNegative)
					{
						if (len == 1)
						{
							if (c >= 216)
								return false;		// Non-canonical - values from -40 to -1 inclusive must be encoded with a single byte
						}
						else
						{
							if ((len == 8) && ((c & 0x80) == 0))
								return false;		// Out of representable range

							if (c == 0xFF)
								return false;		// Non-canonical - superfluous leading 0xFF byte(s)
						}
					}
					else
					{
						if (len == 1)
						{
							if (c <= 199)
								return false;		// Non-canonical - values from 0 to 199 inclusive must be encoded with a single byte
						}
						else
						{
							if ((len == 8) && ((c & 0x80) != 0))
								return false;		// Out of representable range
						
							if (c == 0)
								return false;		// Non-canonical - superfluous leading zero byte(s)
						}
					}

					isLeading = false;
				}

				n = (n << 8) | (byte) c;
			}
			while (--len);
		}

		return true;
	}

}
