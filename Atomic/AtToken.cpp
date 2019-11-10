#include "AtIncludes.h"
#include "AtToken.h"

#include "AtCrypt.h"
#include "AtBaseXY.h"
#include "AtException.h"

namespace At
{
	char const* const Token::Alphabet = Base64::BaseAlphabet;


	Seq Token::Generate(Enc& enc)
	{
		byte buf[RawLen];
		Crypt::GenRandom(buf, RawLen);
		return Tokenize(Seq(buf, RawLen), enc);
	}


	Seq Token::Tokenize(Seq bin, Enc& enc)
	{
		EnsureThrow(bin.n >= 2);

		char const last3[]
			{ Alphabet[bin.p[0] % 62],
			  Alphabet[bin.p[1] % 62],
			  '.',
			  0 };

		return Base64::Encode(bin, enc, Base64::Padding::No, last3, Base64::NewLines::None());
	}


	Seq Token::ToBinary(Seq token, Enc& enc)
	{
		Seq reader = token;
		if (reader.ReadToFirstByteNotOf(Alphabet).n != Len || reader.n != 0)
			return Seq();

		return Base64::Decode(token, enc, "+/=");
	}


	Seq Token::ConvertBackFromBinary(Seq bin, Enc& enc)
	{
		EnsureThrow(bin.n == RawLen);
		Seq token = Base64::Encode(bin, enc, Base64::Padding::No, "???", Base64::NewLines::None());
		EnsureThrow(!token.ContainsByte('?'));
		return token;
	}

}
