#pragma once

#include "AtJsonGrammar.h"
#include "AtStr.h"
#include "AtTime.h"

namespace At
{
	namespace Json
	{
		// Encode

		void EncodeStringPart(Enc& enc, Seq s);

		inline void EncodeString(Enc& enc, Seq s) { enc.Ch('"'); EncodeStringPart(enc, s); enc.Ch('"'); }

		void EncodeArray(Enc& enc, sizet nrValues, std::function<void(Enc&, sizet)> encodeValue);


		// Decode

		struct DecodeErr : public StrErr { DecodeErr(ParseNode const& p, Seq s); };

		void   DecodeString (ParseNode const& p, Enc& enc);
		bool   DecodeBool   (ParseNode const& p);
		uint64 DecodeUInt   (ParseNode const& p);
		int64  DecodeSInt   (ParseNode const& p);
		double DecodeFloat  (ParseNode const& p);
		Time   DecodeTime   (ParseNode const& p);

		void DecodeArray(ParseNode const& p, std::function<void (ParseNode const&)> decodeValue);

		void DecodeObject(ParseNode const& p, std::function<bool (ParseNode const& nameNode, Seq name, ParseNode const& valueNode)> decodePair);
	}
}
