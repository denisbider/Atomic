#pragma once

#include "AtStr.h"
#include "AtException.h"

namespace At
{
	// Supports insertion of arguments into format string using `0, `1, ..., `9.
	// If the ` character needs to appear verbatim in the format string, it can be escaped as "``".

	void AppendFmt(Str& out, Seq format, Seq const** args);

	inline void AppendFmt(Str& out, Seq f, Seq a)               { Seq const* args[2] = { &a, 0 };         AppendFmt(out, f, args); }
	inline void AppendFmt(Str& out, Seq f, Seq a, Seq b)        { Seq const* args[3] = { &a, &b, 0 };     AppendFmt(out, f, args); }
	inline void AppendFmt(Str& out, Seq f, Seq a, Seq b, Seq c) { Seq const* args[4] = { &a, &b, &c, 0 }; AppendFmt(out, f, args); }


	inline Str StrFmt(Seq format, Seq const** args) { Str out; AppendFmt(out, format, args); return out; }

	inline Str StrFmt(Seq f, Seq a)               { Seq const* args[2] = { &a, 0 };         return StrFmt(f, args); }
	inline Str StrFmt(Seq f, Seq a, Seq b)        { Seq const* args[3] = { &a, &b, 0 };     return StrFmt(f, args); }
	inline Str StrFmt(Seq f, Seq a, Seq b, Seq c) { Seq const* args[4] = { &a, &b, &c, 0 }; return StrFmt(f, args); }


	// For convenience

	inline void AppendStr(Str& out, Seq s) { out.Add(s); }
}
