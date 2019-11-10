#include "AtIncludes.h"
#include "AtNumCvt.h"

namespace At
{
	char const* const NumAlphabet::Upper = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char const* const NumAlphabet::Lower = "0123456789abcdefghijklmnopqrstuvwxyz";


	uint HexDigitValue(uint c)
	{
		if (c >= '0' && c <= '9')	return c - '0';
		if (c >= 'A' && c <= 'F')	return 10 + (c - 'A');
		if (c >= 'a' && c <= 'f')	return 10 + (c - 'a');
									return UINT_MAX;
	}



	sizet UInt2Buf(uint64 value, byte* buf, uint base, uint zeroPadWidth, CharCase charCase, uint spacePadWidth)
	{
		byte const* const alphabet = (byte const*) NumAlphabet::Get(charCase);

		if (base < 2)
			base = 2;
		else if (base > 36)
			base = 36;

		byte rbuf[100];
		byte* p = rbuf;

		for (; value; value /= base)
			*p++ = alphabet[value % base];	

		if (p == rbuf)
			*p++ = '0';

		uint unpaddedLen = (uint) (p - rbuf);
		byte const* const origBuf = buf;

		while (spacePadWidth > zeroPadWidth && spacePadWidth > unpaddedLen)
			{ *buf++ = ' '; --spacePadWidth; }

		while (zeroPadWidth > unpaddedLen)
			{ *buf++ = '0'; --zeroPadWidth; }

		do *buf++ = *--p;
		while (p != rbuf);

		return (sizet) (buf - origBuf);
	}



	sizet SInt2Buf(int64 value, byte* buf, AddSign::E addSign, uint base, uint zeroPadWidth, CharCase charCase)
	{
		if (value >= 0)
		{
			sizet signLen {};
			if (addSign == AddSign::Always)
			{
				*buf++ = '+';
				signLen = 1;
			}

			return signLen + UInt2Buf((uint64) value, buf, base, zeroPadWidth, charCase);
		}
		else
		{
			// Do not decrement zeroPadWidth
			*buf++ = '-';

			uint64 uv;
			if (value == INT64_MIN)
				uv = ((uint64) INT64_MAX) + 1;
			else
				uv = (uint64) (-value);

			return 1 + UInt2Buf(uv, buf, base, zeroPadWidth, charCase);
		}
	}


	sizet Dbl2Buf(double value, byte* buf, uint prec)
	{
		byte* bufOrig = buf;
	
		if (value < 0)
		{
			value *= -1.0;
			*buf++ = '-';
		}
	
		uint64 uintPart;
		if (!prec)
			uintPart = (uint64) (value + 0.5);
		else
			uintPart = (uint64) value;

		buf += UInt2Buf(uintPart, buf);
	
		*buf++ = '.';

		if (prec)
		{
			if (prec > 19)
				prec = 19;
	
			double denom = 1.0;
			for (uint i=0; i!=prec; ++i)
				denom *= 10.0;
		
			uint64 fracPart = (uint64) (((value - (double) uintPart) * denom) + 0.5);
			buf += UInt2Buf(fracPart, buf, 10, prec);
		}
	
		return (sizet) (buf - bufOrig);
	}
}
