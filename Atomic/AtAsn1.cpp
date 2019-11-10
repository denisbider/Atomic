#include "AtIncludes.h"
#include "AtAsn1.h"

namespace At
{
	namespace Oid
	{
		char const* const Pkcs1_Rsa = "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01";
	}

	namespace Asn1
	{		

		// BER/DER Length

		sizet EncodeDerLength_Size(uint64 n)
		{
			     if (n <=             0x7FULL) return 1;
			else if (n <=             0xFFULL) return 2;
			else if (n <=           0xFFFFULL) return 3;
			else if (n <=         0xFFFFFFULL) return 4;
			else if (n <=       0xFFFFFFFFULL) return 5;
			else if (n <=     0xFFFFFFFFFFULL) return 6;
			else if (n <=   0xFFFFFFFFFFFFULL) return 7;
			else if (n <= 0xFFFFFFFFFFFFFFULL) return 8;
			else                               return 9;
		}
		

		void EncodeDerLength(Enc& enc, uint64 n)
		{
			if (n <= 0x7F)
				enc.Byte((byte) (n & 0x7F));
			else
			{
				sizet      len    = EncodeDerLength_Size(n);
				Enc::Write write  = enc.IncWrite(len);
				byte*      pStart = write.Ptr();
				byte*      pWrite = pStart;

				sizet nrRemaining = len - 1;
				*pWrite++ = (byte) (0x80 | (nrRemaining & 0x7F));

				do
				{
					--nrRemaining;
					*pWrite++ = (byte) ((n >> (8*nrRemaining)) & 0xFF);
				}
				while (nrRemaining != 0);

				write.AddUpTo(pWrite);
				EnsureThrow(write.Met());
			}
		}


		bool DecodeBerLength(Seq& s, uint64& n, NonCanon nc)
		{
			if (!s.n)
				return false;

			if (s.p[0] <= 0x7F)
				n = s.ReadByte();
			else
			{
				sizet nrRemaining = (s.p[0] & 0x7FU);
				if (nrRemaining == 0)      return false;						// Indefinite length
				if (s.n < 1 + nrRemaining) return false;						// Not enough input to decode length

				if (nc != NonCanon::Permit)
				{
					if (s.p[1] == 0)                        return false;		// Non-canonical: can't have leading zero bytes
					if (nrRemaining == 1 && s.p[1] <= 0x7F) return false;		// Non-canonical: should be encoded as single byte
					if (nrRemaining > 8)                    return false;		// Too large, cannot be represented
				}

				n = 0;
				for (sizet i=1; i<=nrRemaining; ++i)
				{
					if ((n & 0xFF00000000000000ULL) != 0) return false;			// Too large, cannot be represented
					n = ((n << 8) | s.p[i]);
				}

				s.DropBytes(1 + nrRemaining);
			}

			return true;
		}



		// INTEGER

		namespace
		{
			sizet EncodeDerUInteger_InnerLen(Seq& reader)
			{
				while (reader.FirstByte() == 0)
					reader.DropByte();

				sizet innerLen = reader.n;
				if (!innerLen || reader.FirstByte() >= 0x80)
					++innerLen;

				return innerLen;
			}

		}	// anon


		sizet EncodeDerUInteger_Size(Seq bytes)
		{
			sizet innerLen = EncodeDerUInteger_InnerLen(bytes);
			return 1 + EncodeDerLength_Size(innerLen) + innerLen;
		}
		
		
		void EncodeDerUInteger(Enc& enc, Seq bytes)
		{
			sizet innerLen = EncodeDerUInteger_InnerLen(bytes);
			EncodeByte(enc, 2);
			EncodeDerLength(enc, innerLen);
			if (innerLen != bytes.n)
			{
				EnsureThrow(innerLen == bytes.n + 1);
				EncodeByte(enc, 0);
			}
			if (bytes.n)
				EncodeVerbatim(enc, bytes);
		}


		bool DecodeBerUInteger(Seq& s, Seq& bytes, NonCanon nc, sizet maxBytes)
		{
			Seq reader = s;
			uint64 innerLen;

			if (reader.ReadByte() != 0x02)              return false;
			if (!DecodeBerLength(reader, innerLen, nc)) return false;
			if (innerLen == 0)                          return false;
			if (innerLen > maxBytes)                    return false;
			if (innerLen > reader.n)                    return false;
			if (reader.p[0] >= 0x80)                    return false;	// Encoded integer is negative

			bytes = reader.ReadBytes((sizet) innerLen);
			if (!bytes.p[0])
			{
				if (nc != NonCanon::Permit && bytes.n >= 2 && bytes.p[1] < 0x80)
					return false;
					
				while (bytes.n >= 2 && !bytes.p[0])
					bytes.DropByte();
			}
			
			s = reader;
			return true;
		}



		// BIT STRING

		sizet EncodeDerBitStrHeader_Size(sizet innerLen)
		{
			return 1 + EncodeDerLength_Size(1 + innerLen) + 1;
		}


		void EncodeDerBitStrHeader(Enc& enc, sizet innerLen)
		{
			EncodeByte      (enc, 0x03);
			EncodeDerLength (enc, 1 + innerLen);
			EncodeByte      (enc, 0);
		}


		bool DecodeBerBitStr(Seq& s, Seq& content, uint& unusedBits, NonCanon nc, sizet maxContentLen)
		{
			Seq reader = s;
			uint64 contentLen;
			if (reader.ReadByte() != 0x03)                return false;
			if (!DecodeBerLength(reader, contentLen, nc)) return false;
			if (contentLen < 1)                           return false;
			if (contentLen > reader.n)                    return false;
			if (!DecodeByte(reader, unusedBits))          return false;
			if (--contentLen > maxContentLen)             return false;
			if (unusedBits != 0)
			{
				if (contentLen < (unusedBits + 7) / 8)    return false;
				if (nc != NonCanon::Permit)
				{
					if (unusedBits > 7)                   return false;
					uint lastByte = reader.p[((sizet) contentLen) - 1];
					uint mask     = (1U << unusedBits) - 1U;
					if ((lastByte & mask) != 0)           return false;
				}
			}
			content.p = reader.p;
			content.n = (sizet) contentLen;
			s = reader.DropBytes(content.n);
			return true;
		}
		
		
		bool DecodeBerBitStr(Seq& s, Seq& content, NonCanon nc, sizet maxContentLen)
		{
			Seq reader = s;
			uint unusedBits;
			if (!DecodeBerBitStr(reader, content, unusedBits, nc, maxContentLen)) return false;
			if (unusedBits != 0) return false;
			s = reader;
			return true;
		}



		// NULL

		void EncodeDerNull(Enc& enc)
		{
			EncodeVerbatim(enc, Seq("\x05\x00", 2));
		}


		bool DecodeDerNull(Seq& s, NonCanon nc)
		{
			Seq reader = s;
			uint64 contentLen;
			if (reader.ReadByte() != 0x05)                return false;
			if (!DecodeBerLength(reader, contentLen, nc)) return false;
			if (contentLen != 0)                          return false;
			s = reader;
			return true;
		}



		// OBJECT IDENTIFIER

		sizet EncodeDerOidHeader_Size(sizet innerLen)
		{
			return 1 + EncodeDerLength_Size(innerLen);
		}


		void EncodeDerOidHeader(Enc& enc, sizet innerLen)
		{
			EncodeByte      (enc, 0x06);
			EncodeDerLength (enc, innerLen);
		}


		bool DecodeBerOid(Seq&s, Seq& content, NonCanon nc, sizet maxContentLen)
		{
			Seq reader = s;
			uint64 contentLen;
			if (reader.ReadByte() != 0x06)                return false;
			if (!DecodeBerLength(reader, contentLen, nc)) return false;
			if (contentLen < 1)                           return false;
			if (contentLen > reader.n)                    return false;
			if (contentLen > maxContentLen)               return false;
			content.p = reader.p;
			content.n = (sizet) contentLen;
			s = reader.DropBytes(content.n);
			return true;
		}



		// SEQUENCE

		sizet EncodeDerSeqHeader_Size(sizet innerLen)
		{
			return 1 + EncodeDerLength_Size(innerLen);
		}
		
		
		void EncodeDerSeqHeader(Enc& enc, sizet innerLen)
		{
			EncodeByte      (enc, 0x30);
			EncodeDerLength (enc, innerLen);
		}


		bool DecodeBerSeq(Seq& s, Seq& content, NonCanon nc, sizet maxContentLen)
		{
			Seq reader = s;
			uint64 contentLen;
			if (reader.ReadByte() != 0x30)                return false;
			if (!DecodeBerLength(reader, contentLen, nc)) return false;
			if (contentLen > reader.n)                    return false;
			if (contentLen > maxContentLen)               return false;
			content.p = reader.p;
			content.n = (sizet) contentLen;
			s = reader.DropBytes(content.n);
			return true;
		}

	}
}
