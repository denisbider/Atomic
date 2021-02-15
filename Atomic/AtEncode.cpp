#include "AtIncludes.h"
#include "AtEncode.h"

#include "AtNumCvt.h"

namespace At
{

	// Byte

	bool DecodeByte(Seq& s, uint& b)
	{
		if (s.n < 1)
			return false;

		b = *s.p;
		s.DropByte();
		return true;
	}



	// UInt16

	void EncodeUInt16(Enc& enc, unsigned int n)
	{
		enc.Byte((byte) ((n >>  8) & 0xFF));
		enc.Byte((byte) ((n      ) & 0xFF));
	}

	byte* EncodeUInt16LE_Ptr (byte* p, unsigned int n)
	{
		
		*p++ = (byte) ((n      ) & 0xFF);
		*p++ = (byte) ((n >> 8 ) & 0xFF);
		return p;
	}

	void EncodeUInt16LE(Enc& enc, unsigned int n)
	{
		enc.Byte((byte) ((n      ) & 0xFF));
		enc.Byte((byte) ((n >> 8 ) & 0xFF));
	}

	bool DecodeUInt16(Seq& s, unsigned int& n)
	{
		if (s.n < 2)
			return false;

		n = (((unsigned int) s.p[0]) <<  8) |
			(((unsigned int) s.p[1])      );

		s.DropBytes(2);
		return true;
	}

	bool DecodeUInt16LE(Seq& s, unsigned int& n)
	{
		if (s.n < 2)
			return false;

		n = (((unsigned int) s.p[0])      ) |
			(((unsigned int) s.p[1]) <<  8);

		s.DropBytes(2);
		return true;
	}



	// UInt32

	byte* EncodeUInt32_Ptr(byte* p, uint32 n)
	{
		*p++ = (byte) ((n >> 24) & 0xFF);
		*p++ = (byte) ((n >> 16) & 0xFF);
		*p++ = (byte) ((n >>  8) & 0xFF);
		*p++ = (byte) ((n      ) & 0xFF);
		return p;
	}

	byte* EncodeUInt32LE_Ptr(byte* p, uint32 n)
	{
		*p++ = (byte) ((n      ) & 0xFF);
		*p++ = (byte) ((n >>  8) & 0xFF);
		*p++ = (byte) ((n >> 16) & 0xFF);
		*p++ = (byte) ((n >> 24) & 0xFF);
		return p;
	}

	void EncodeUInt32(Enc& enc, uint32 n)
	{
		Enc::Write write = enc.IncWrite(4);
		EncodeUInt32_Ptr(write.Ptr(), n);
		write.Add(4);
	}

	void EncodeUInt32LE(Enc& enc, uint32 n)
	{
		Enc::Write write = enc.IncWrite(4);
		EncodeUInt32LE_Ptr(write.Ptr(), n);
		write.Add(4);
	}

	byte const* DecodeUInt32_Ptr(byte const* p, uint32& n)
	{
		n = (((uint32) p[0]) << 24) |
			(((uint32) p[1]) << 16) |
			(((uint32) p[2]) <<  8) |
			(((uint32) p[3])      );
		return p + 4;
	}

	byte const* DecodeUInt32LE_Ptr(byte const* p, uint32& n)
	{
		n = (((uint32) p[0])      ) |
			(((uint32) p[1]) <<  8) |
			(((uint32) p[2]) << 16) |
			(((uint32) p[3]) << 24);
		return p + 4;
	}

	bool DecodeUInt32(Seq& s, uint32& n)
	{
		if (s.n < 4)
			return false;

		DecodeUInt32_Ptr(s.p, n);
		s.DropBytes(4);
		return true;
	}

	bool DecodeUInt32LE(Seq& s, uint32& n)
	{
		if (s.n < 4)
			return false;

		DecodeUInt32LE_Ptr(s.p, n);
		s.DropBytes(4);
		return true;
	}



	// UInt64

	byte* EncodeUInt64_Ptr(byte* p, uint64 n)
	{
		*p++ = (byte) ((n >> 56) & 0xFF);
		*p++ = (byte) ((n >> 48) & 0xFF);
		*p++ = (byte) ((n >> 40) & 0xFF);
		*p++ = (byte) ((n >> 32) & 0xFF);
		*p++ = (byte) ((n >> 24) & 0xFF);
		*p++ = (byte) ((n >> 16) & 0xFF);
		*p++ = (byte) ((n >>  8) & 0xFF);
		*p++ = (byte) ((n      ) & 0xFF);
		return p;
	}

	byte* EncodeUInt64LE_Ptr(byte* p, uint64 n)
	{
		*p++ = (byte) ((n      ) & 0xFF);
		*p++ = (byte) ((n >>  8) & 0xFF);
		*p++ = (byte) ((n >> 16) & 0xFF);
		*p++ = (byte) ((n >> 24) & 0xFF);
		*p++ = (byte) ((n >> 32) & 0xFF);
		*p++ = (byte) ((n >> 40) & 0xFF);
		*p++ = (byte) ((n >> 48) & 0xFF);
		*p++ = (byte) ((n >> 56) & 0xFF);
		return p;
	}

	void EncodeUInt64(Enc& enc, uint64 n)
	{
		Enc::Write write = enc.IncWrite(8);
		EncodeUInt64_Ptr(write.Ptr(), n);
		write.Add(8);
	}

	void EncodeUInt64LE(Enc& enc, uint64 n)
	{
		Enc::Write write = enc.IncWrite(8);
		EncodeUInt64LE_Ptr(write.Ptr(), n);
		write.Add(8);
	}

	byte const* DecodeUInt64_Ptr(byte const* p, uint64& n)
	{
		n = (((uint64) p[0]) << 56) |
			(((uint64) p[1]) << 48) |
			(((uint64) p[2]) << 40) |
			(((uint64) p[3]) << 32) |
			(((uint64) p[4]) << 24) |
			(((uint64) p[5]) << 16) |
			(((uint64) p[6]) <<  8) |
			(((uint64) p[7])      );
		return p + 8;
	}

	byte const* DecodeUInt64LE_Ptr(byte const* p, uint64& n)
	{
		n = (((uint64) p[0])      ) |
			(((uint64) p[1]) <<  8) |
			(((uint64) p[2]) << 16) |
			(((uint64) p[3]) << 24) |
			(((uint64) p[4]) << 32) |
			(((uint64) p[5]) << 40) |
			(((uint64) p[6]) << 48) |
			(((uint64) p[7]) << 56);
		return p + 8;
	}

	bool DecodeUInt64(Seq& s, uint64& n)
	{
		if (s.n < 8)
			return false;

		DecodeUInt64_Ptr(s.p, n);
		s.DropBytes(8);
		return true;
	}

	bool DecodeUInt64LE(Seq& s, uint64& n)
	{
		if (s.n < 8)
			return false;

		DecodeUInt64LE_Ptr(s.p, n);
		s.DropBytes(8);
		return true;
	}



	// Double

	bool DecodeDouble(Seq& s, double& v)
	{
		if (s.n < 8)
			return false;

		memcpy(&v, s.p, 8);
		s.DropBytes(8);
		return true;
	}



	// SortDouble

	void EncodeSortDouble(Enc& enc, double v)
	{
		// Doubles are mostly sorted, except that:
		// - x86 systems store them least significant byte first
		// - negative values have to be inverted for proper sorting

		Enc::Write write = enc.IncWrite(8);
		byte* p = write.Ptr();

		byte const* d = (byte const*) &v;
		if ((d[7] & 0x80) == 0)
		{
			// Positive
			*p++ = 0x80U | d[7];

			sizet i = 6;
			while (true)
			{
				*p++ = d[i];
				if (i == 0)
					break;
				--i;
			}
		}
		else
		{
			// Negative - needs to be inverted
			*p++ = 0x7fU - (d[7] & 0x7fU);

			sizet i = 6;
			while (true)
			{
				*p++ = 0xffU - d[i];
				if (i == 0)
					break;
				--i;
			}
		}

		write.Add(8);
	}

	bool DecodeSortDouble(Seq& s, double& v)
	{
		if (s.n < 8)
			return false;

		byte* d = (byte*) &v;
	
		if ((s.p[0] & 0x80) != 0)
		{
			// Positive
			*d++ = (s.p[0] & 0x7fU);
		
			sizet i = 7;
			while (true)
			{
				*d++ = s.p[i];
				if (i == 1)
					break;
				--i;
			}
		}
		else
		{
			// Negative - needs to be inverted
			*d++ = (0x7fU - s.p[0]) | 0x80U;

			sizet i=7;
			while (true)
			{
				*d++ = 0xffU - s.p[i];
				if (i == 1)
					break;
				--i;
			}
		}

		s.DropBytes(8);
		return true;
	}



	// VarStr

	void EncodeVarStr(Enc& enc, Seq data)
	{
		EncodeVarUInt64(enc, data.n);
		enc.Add(data);
	}

	bool CanTryDecodeVarStr(Seq s, sizet maxLen)
	{
		if (!CanTryDecodeVarSInt64(s))
			return false;
		
		uint64 dataSize;
		if (!DecodeVarUInt64(s, dataSize))
			return true;	// Decoding can proceed, and will fail

		return s.n > maxLen || s.n >= dataSize;
	}

	bool DecodeVarStr(Seq& s, Seq& data, sizet maxLen)
	{
		uint64 dataSize;
		if (!DecodeVarUInt64(s, dataSize))
			return false;

		if (dataSize > maxLen || dataSize > s.n)
			return false;

		data.p = s.p;
		data.n = (sizet) dataSize;
		s.DropBytes(data.n);
		return true;
	}



	// SortStr

	sizet EncodeSortStr_Size(Seq data)
	{
		sizet encodedSize { 2 };
	
		sizet nrZeroes {};
		while (true)
		{
			uint c { data.ReadByte() };
			if (c == 0)
				++nrZeroes;
			else
			{
				if (nrZeroes != 0)
				{
					encodedSize += 2 * ((nrZeroes + 254) / 255);
					nrZeroes = 0;
				}

				if (c == UINT_MAX)
					break;

				++encodedSize;
			}
		}

		return encodedSize;
	}

	void EncodeSortStr(Enc& enc, Seq data)
	{
		sizet nrZeroes {};
		while (true)
		{
			uint c { data.ReadByte() };
			if (c == 0)
				++nrZeroes;
			else
			{
				while (nrZeroes != 0)
				{
					sizet nrZeroesToEncode { nrZeroes };
					if (nrZeroesToEncode > 255)
						nrZeroesToEncode = 255;

					enc.Byte(0);
					enc.Byte((byte) nrZeroesToEncode);
				
					nrZeroes -= nrZeroesToEncode;
				}

				if (c == UINT_MAX)
					break;

				enc.Byte((byte) c);
			}
		}

		enc.Byte(0);
		enc.Byte(0);
	}

	bool DecodeSortStr(Seq& s, Str& data)
	{
		data.Clear().ReserveAtLeast(PickMin<sizet>(s.n, 200));

		while (true)
		{
			uint c { s.ReadByte() };
			if (c == UINT_MAX)
				return false;

			if (c != 0)
				data.Byte((byte) c);
			else
			{
				c = s.ReadByte();
				if (c == UINT_MAX)
					return false;
				if (c == 0)
					return true;

				data.Bytes((sizet) c, 0);
			}
		}
	}
}
