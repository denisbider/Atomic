#include "AtIncludes.h"
#include "AtStrFmt.h"

namespace At
{
	void AppendFmt(Str& out, Seq format, Seq const** args)
	{
		sizet argCount = 0;
		for (Seq const** arg=args; *arg; ++arg)
			++argCount;

		sizet expectedLen = out.Len();
		Seq scanAhead = format;
		if (scanAhead.n)
			while (true)
			{
				expectedLen += scanAhead.ReadToByte('`').n;
				if (!scanAhead.n)
					break;

				if (scanAhead.StartsWithExact("``"))
				{
					++expectedLen;
					scanAhead.DropBytes(2);
				}
				else
				{
					scanAhead.DropByte();

					uint argIdx { scanAhead.ReadByte() };
					if (argIdx == UINT_MAX)
						throw ZLitErr("StrFmt: Unexpected end of format string");

					if (argIdx < '0')	
						throw ZLitErr("StrFmt: Invalid format string index");

					argIdx -= '0';
					if (argIdx >= argCount)
						throw ZLitErr("StrFmt: Format string index out of bounds");
			
					expectedLen += args[argIdx]->n;
				}
			}
	
		out.ReserveExact(expectedLen);

		if (format.n)
			while (true)
			{
				out.Add(format.ReadToByte('`'));
				if (!format.n)
					break;

				if (format.StartsWithExact("``"))
				{
					out.Ch('`');
					format.DropBytes(2);
				}
				else
				{
					format.DropByte();
					uint argIdx { format.ReadByte() };
					EnsureThrow(argIdx != UINT_MAX && argIdx >= '0');
					argIdx -= '0';
					EnsureThrow(argIdx < argCount);
					out.Add(*(args[argIdx]));
				}
			}
	
		EnsureAbort(out.Len() == expectedLen);
	}
}
