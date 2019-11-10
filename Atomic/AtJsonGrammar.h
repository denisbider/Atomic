#pragma once

#include "AtAscii.h"
#include "AtParse.h"

namespace At
{
	namespace Json
	{
		DECL_RUID(CharsNonEsc)
		DECL_RUID(EscSpecial)
		DECL_RUID(EscUnicode)
		DECL_RUID(Value)
		DECL_RUID(String)
		DECL_RUID(True)
		DECL_RUID(False)
		DECL_RUID(Null)
		DECL_RUID(Number)
		DECL_RUID(Array)
		DECL_RUID(Pair)
		DECL_RUID(Object)

		bool C_Value  (ParseNode& p);
		bool C_Array  (ParseNode& p);
		bool C_Object (ParseNode& p);
		bool C_Json   (ParseNode& p);
	}
}
