#pragma once

#include "AtParse.h"

namespace At
{
	namespace Css
	{
		DECL_RUID(WhitespaceTkn)
		DECL_RUID(Comment)
		DECL_RUID(WsComments)
		DECL_RUID(StrChars)
		DECL_RUID(SqStrChars)
		DECL_RUID(DqStrChars)
		DECL_RUID(StrEscSeq)
		DECL_RUID(StrLineCont)
		DECL_RUID(String)
		DECL_RUID(SqString)
		DECL_RUID(DqString)
		DECL_RUID(Name)
		DECL_RUID(Ident)
		DECL_RUID(HashTkn)
		DECL_RUID(HashTknUnr)
		DECL_RUID(HashTknId)
		DECL_RUID(Number)
		DECL_RUID(DimensionTkn)
		DECL_RUID(PercentageTkn)
		DECL_RUID(CDO)
		DECL_RUID(CDC)
		DECL_RUID(UrlInner)
		DECL_RUID(UrlTkn)
		DECL_RUID(FunctionTkn)
		DECL_RUID(AtKwTkn)
		DECL_RUID(DelimTkn)

		bool C_Tokens(ParseNode& p);
	}
}
