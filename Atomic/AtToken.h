#pragma once

#include "AtStr.h"


namespace At
{

	struct Token
	{
		// Tokenized encoding is modified (lossy) Base64 without padding. Length is as many bytes as actually needed to encode the bits.
		// Encoding loss occurs because two symbols in the output alphabet (a-zA-Z0-9) are reused so that all tokens can be alphanumeric.
		// The exact two symbols that are reused depend on the first two bytes of the original binary input for each token.
		//
		// If the encoding was not lossy, the entropy of a token would be 18*8 = 144 bits. However, because each symbol in the
		// 24-character output can only represent 62 values instead of 64, the entropy of a token is 142.9 bits (10^43 possible values).
		//
		// Because tokenized encoding is lossy, ToBinary(Tokenize(bin)) is NOT an identity function for many binary inputs.
		// However, Tokenize(ToBinary(token)) IS an identity function for tokens previously obtained from Tokenize or Generate.
		// For improved safety, any such round-trip should use ConvertBackFromBinary(ToBinary(token)), which verifies the conversion.

		static char const* const Alphabet;
		enum { RawLen = 18, Len = 24 };

		constexpr static sizet TokenizedLen(sizet binLen) { return ((8*binLen) + 5) / 6; }

		static Seq Generate(Enc& enc);
		static inline Str Generate() { Str token; Generate(token); return std::move(token); }

		// See notes: because tokenized encoding is lossy, ToBinary(Tokenize(bin)) is NOT an identity function for many binary inputs.
		static Seq Tokenize(Seq bin, Enc& enc);
		static inline Str Tokenize(Seq bin) { Str tokenized; Tokenize(bin, tokenized); return std::move(tokenized); }

		static Seq ToBinary(Seq token, Enc& enc);
		static inline Str ToBinary(Seq token) { Str bin; ToBinary(token, bin); return std::move(bin); }

		// Use ONLY for binary data that previously underwent Tokenize() => ToBinary().
		// Input not previously processed with Tokenize() will not convert correctly.
		static Seq ConvertBackFromBinary(Seq bin, Enc& enc);
		static inline Str ConvertBackFromBinary(Seq bin) { Str token; ConvertBackFromBinary(bin, token); return std::move(token); }
	};

}
