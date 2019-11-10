#include "AtIncludes.h"
#include "AtQuoteStr.h"

namespace At
{
	Str QuoteStr(Seq s)
	{
		Str r;
		r.Ch('"');
	
		while (true)
		{
			Seq chunk = s.ReadToFirstByteOf("\\\"\r\n");
			r.Add(chunk);

			uint c { s.ReadByte() };
			if (c == UINT_MAX)
				break;

			r.Ch('\\');

			switch (c)
			{
			case '\r': r.Ch('r'); break;
			case '\n': r.Ch('n'); break;
			default:   r.Byte((byte) c); break;
			}
		}
	
		r.Ch('"');
		return r;
	}


	Str ReadQuotedStr(Seq& s)
	{
		if (!s.n || s.p[0] != '"')
			return s;
	
		Str r;
		s.DropByte();

		while (true)
		{
			Seq chunk = s.ReadToFirstByteOf("\\\"");
			r.Add(chunk);

			uint c { s.ReadByte() };
			if (c == UINT_MAX || c == '"')
				break;

			c = s.ReadByte();
			if (c == UINT_MAX)
				break;

			switch (c)
			{
			case 'r': r.Ch('\r'); break;
			case 'n': r.Ch('\n'); break;
			default:  r.Byte((byte) c); break;
			}
		}

		return r;
	}
}
