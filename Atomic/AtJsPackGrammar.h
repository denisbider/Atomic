#pragma once

#include "AtIncludes.h"
#include "AtParse.h"

namespace At
{
namespace JsPack
{

	DECL_RUID(WsNlComments)
	DECL_RUID(StrChars)
	DECL_RUID(SqStrChars)
	DECL_RUID(DqStrChars)
	DECL_RUID(StrEscSeq)
	DECL_RUID(StrLineCont)
	DECL_RUID(String)
	DECL_RUID(SqString)
	DECL_RUID(DqString)
	DECL_RUID(Other)
	DECL_RUID(TmplChars)
	DECL_RUID(TmplEsc)
	DECL_RUID(TmplInsertOpen)
	DECL_RUID(TmplInsertBody)
	DECL_RUID(TmplInsert)
	DECL_RUID(Template)

	bool C_Script(ParseNode& p);

}
}
