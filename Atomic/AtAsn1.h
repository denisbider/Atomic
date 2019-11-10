#pragma once

#include "AtEncode.h"


namespace At
{
	namespace Oid
	{
		extern char const* const Pkcs1_Rsa;
	}

	namespace Asn1
	{

		struct Id { enum E {
				Integer  = 0x02,
				Sequence = 0x30,
			}; };


		enum class NonCanon { Reject, Permit };


		sizet EncodeDerLength_Size(uint64 n);
		void  EncodeDerLength(Enc& enc, uint64 n);
		bool  DecodeBerLength(Seq& s, uint64& n, NonCanon nc);						// Returns false if indefinite length

		sizet EncodeDerUInteger_Size(Seq bytes);
		void  EncodeDerUInteger(Enc& enc, Seq bytes);
		bool  DecodeBerUInteger(Seq& s, Seq& bytes, NonCanon nc, sizet maxBytes);	// Returns false if the integer is negative

		sizet EncodeDerBitStrHeader_Size(sizet innerLen);
		void  EncodeDerBitStrHeader(Enc& enc, sizet innerLen);
		bool  DecodeBerBitStr(Seq& s, Seq& content, uint& unusedBits, NonCanon nc, sizet maxContentLen);
		bool  DecodeBerBitStr(Seq& s, Seq& content, NonCanon nc, sizet maxContentLen);

		enum { EncodeDerNull_Size = 2 };
		void  EncodeDerNull(Enc& enc);
		bool  DecodeDerNull(Seq& s, NonCanon nc);

		sizet EncodeDerOidHeader_Size(sizet innerLen);
		void  EncodeDerOidHeader(Enc& enc, sizet innerLen);
		bool  DecodeBerOid(Seq&s, Seq& content, NonCanon nc, sizet maxContentLen);

		sizet EncodeDerSeqHeader_Size(sizet innerLen);
		void  EncodeDerSeqHeader(Enc& enc, sizet innerLen);
		bool  DecodeBerSeq(Seq& s, Seq& content, NonCanon nc, sizet maxContentLen);
	}
}

