#include "AtIncludes.h"
#include "AtEnc.h"

#include "AtImfGrammar.h"
#include "AtStr.h"
#include "AtUtf8.h"


namespace At
{

	Enc& Enc::Add(byte const* ptr, sizet count)
	{
		if (count)
		{
			EnsureThrow(ptr);
			EnsureThrow(count < SIZE_MAX - m_len);
			sizet newLen = m_len + count;

			Vec<byte>::ReserveAtLeast(newLen);
			memcpy(Ptr() + m_len, ptr, count);
			m_len = newLen;
		}

		return *this;
	}


	template <typename SeqOrStr>
	Enc& Enc::AddN(typename Vec<SeqOrStr> const& container, Seq separator)
	{
		bool any {};
		sizet inc {};
		for (SeqOrStr const& s : container)
		{
			if (!any) any = true; else inc += separator.n;
			inc += s.Len();
		}

		ReserveInc(inc);

		any = false;
		for (SeqOrStr const& s : container)
		{
			if (!any) any = true; else Add(separator);
			Add(s);
		}

		return *this;
	}

	template Enc& Enc::AddN<Seq>(Vec<Seq> const& container, Seq separator);
	template Enc& Enc::AddN<Str>(Vec<Str> const& container, Seq separator);


	Enc& Enc::Utf8Char(uint c)
	{
		ReserveInc(Utf8::MaxBytesPerChar);
		uint addLen = Utf8::WriteCodePoint(Ptr() + m_len, c);
		m_len += addLen;
		return *this;
	}


	Enc& Enc::Bytes(sizet count, byte ch)
	{
		if (count)
		{
			EnsureThrow(count < SIZE_MAX - m_len);
			sizet newLen = m_len + count;

			Vec<byte>::ReserveAtLeast(newLen);
			memset(Ptr() + m_len, ch, count);
			m_len = newLen;
		}

		return *this;
	}


	Enc& Enc::Word16LE(uint16 v)
	{
		return Byte((byte) ( v       & 0xff))
			  .Byte((byte) ((v >> 8) & 0xff));
	}
	

	Enc& Enc::Word32LE(uint32 v)
	{
		return Byte((byte) ( v        & 0xff))
			  .Byte((byte) ((v >>  8) & 0xff))
			  .Byte((byte) ((v >> 16) & 0xff))
			  .Byte((byte) ((v >> 24) & 0xff));
	}


	Enc& Enc::Hex(uint c, CharCase cc)
	{
		char const* a { NumAlphabet::Get(cc) };
		return Ch(a[(c >> 4) & 0x0f]).Ch(a[c & 0x0f]);
	}


	Enc& Enc::Hex(Seq s, CharCase cc, byte sep)
	{
		uint c;
		char const* a { NumAlphabet::Get(cc) };

		if (!sep)
		{
			ReserveAtLeast(Len() + (2 * s.n));
			while ((c = s.ReadByte()) != UINT_MAX)
				Ch(a[(c >> 4) & 0x0f]).Ch(a[c & 0x0f]);
		}
		else
		{
			ReserveAtLeast(Len() + (3 * s.n));

			bool first { true };
			while ((c = s.ReadByte()) != UINT_MAX)
			{
				if (first)
					first = false;
				else
					Byte(sep);

				Ch(a[(c >> 4) & 0x0f]).Ch(a[c & 0x0f]);
			}
		}

		return *this;
	}


	Enc& Enc::HexDump(Seq s, sizet indent, sizet bytesPerLine, CharCase cc, Seq newLine)
	{
		// Sample output with indent=0, bytesPerLine=16, newLine="\r\n":
		// "00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00   ................\r\n"

		if (bytesPerLine < 4)
			bytesPerLine = 4;

		sizet nrLines   { (s.n + (bytesPerLine-1)) / bytesPerLine };
		sizet lineWidth { indent + (bytesPerLine*4) + (bytesPerLine/4) + 1 };

		ReserveInc(nrLines * (lineWidth + newLine.n));

		char const* a { NumAlphabet::Get(cc) };
		while (s.n)
		{
			sizet hexStart { Len() + indent };
			sizet rawStart { Len() + lineWidth - bytesPerLine };
			Chars(lineWidth, ' ').Add(newLine);

			for (sizet i=0; i!=bytesPerLine; ++i)
			{
				uint c { s.ReadByte() };
				if (c == UINT_MAX)
					break;

				sizet hexPos { hexStart + (3*i) + (i/4) };
				Ptr()[hexPos  ] = (byte) a[(c >> 4) & 0x0f];
				Ptr()[hexPos+1] = (byte) a[ c       & 0x0f];

				Ptr()[rawStart+i] = (Ascii::Is7Bit(c) && !Ascii::IsControl(c)) ? ((byte) c) : (byte) '.';
			}
		}

		return *this;
	}


	Enc& Enc::UInt(uint64 v, uint base, uint zeroPadWidth, CharCase charCase, uint spacePadWidth)
	{
		enum { MaxAddBytes = 99 };
		EnsureThrow(zeroPadWidth  < MaxAddBytes);
		EnsureThrow(spacePadWidth < MaxAddBytes);

		byte buf[MaxAddBytes];
		Seq t { buf, UInt2Buf(v, buf, base, zeroPadWidth, charCase, spacePadWidth) };
		EnsureAbort(t.n <= MaxAddBytes);

		return Add(t);
	}
	

	Enc& Enc::SInt(int64 v, AddSign::E addSign, uint base, uint zeroPadWidth, CharCase charCase)
	{
		enum { MaxAddBytes = 99 };
		EnsureThrow(zeroPadWidth < MaxAddBytes);

		byte buf[MaxAddBytes];
		Seq t { buf, SInt2Buf(v, buf, addSign, base, zeroPadWidth, charCase) };
		EnsureAbort(t.n <= MaxAddBytes);

		return Add(t);
	}


	Enc& Enc::Dbl(double v, uint prec)
	{
		enum { MaxAddBytes = 99};

		byte buf[MaxAddBytes];
		Seq t { buf, Dbl2Buf (v, buf, prec) };
		EnsureAbort(t.n <= MaxAddBytes);

		return Add(t);
	}


	Enc& Enc::ErrCode(int64 v)
	{
		if (v >= INT32_MIN && v < INT16_MIN)
			return Add("0x").UInt(((uint64) v) & 0xFFFFFFFFU, 16, 8);

		if (v <= INT16_MAX)
			return SInt(v);
		
		return Add("0x").UInt((uint64) v, 16, 8); 
	}


	Enc& Enc::UIntDecGrp(uint64 v)
	{
		uint64 vFrac = (v / 1000);
		uint64 groupSize = 1;
		while (groupSize <= vFrac)
			groupSize *= 1000;

		uint zeroPadWidth {};
		while (true)
		{
			uint64 groupValue = (v / groupSize);
			UInt(groupValue, 10, zeroPadWidth);
			if (1 == groupSize)
				break;

			v %= groupSize;
			groupSize /= 1000;
			zeroPadWidth = 3;
			Ch(',');
		}

		return *this;
	}


	Enc& Enc::UIntUnitsEx(uint64 v, Slice<Units::Unit> units, Units::Unit const*& largestFitUnit)
	{
		ValueInUnits vu = ValueInUnits::Make(v, units, largestFitUnit);

		UIntDecGrp(vu.m_whole);
		if (vu.m_fracWidth)
			Ch('.').UInt(vu.m_frac, 10, vu.m_fracWidth);

		return *this;
	}


	Enc& Enc::UIntUnits(uint64 v, Slice<Units::Unit> units)
	{
		Units::Unit const* largestFitUnit {};
		UIntUnitsEx(v, units, largestFitUnit);

		if (largestFitUnit && largestFitUnit->m_zName && largestFitUnit->m_zName[0])
			Ch(' ').Add(largestFitUnit->m_zName);

		return *this;
	}


	Enc& Enc::Lower(Seq source)
	{
		sizet len = Len();
		ResizeAtLeast(len + source.n);
		byte* p = Ptr() + len;
		while (source.n--)
			*p++ = ToLower(*source.p++);
		return *this;
	}


	Enc& Enc::Upper(Seq source)
	{
		sizet len = Len();
		ResizeAtLeast(len + source.n);
		byte* p = Ptr() + len;
		while (source.n--)
			*p++ = ToUpper(*source.p++);
		return *this;
	}


	Enc& Enc::UrlDecode(Seq in)
	{
		// Warning: decoded value is binary; it cannot be assumed to be valid in any code page!
		ReserveAtLeast(Len() + in.n);
	
		for (sizet i=0; i!=in.n; ++i)
			if (in.p[i] == '+')
				Ch(' ');
			else if (in.p[i] != '%')
				Byte(in.p[i]);
			else if (in.n - i < 3)
				break;
			else
			{
				uint d1 { HexDigitValue(in.p[++i]) };
				uint d2 { HexDigitValue(in.p[++i]) };
				if (d1 != UINT_MAX && d2 != UINT_MAX)
					Byte((byte) ((d1 << 4) | d2));
			}

		return *this;
	}


	Enc& Enc::UrlEncode(Seq text)
	{
		while (text.n)
		{
			Seq segment { text.ReadToFirstByteNotOfType(Ascii::IsAlphaNum) };
			Add(segment);

			if (!text.n)
				break;

			byte c { text.p[0] };
			Ch('%');
			Ch(NumAlphabet::Upper[(c >> 4) & 0x0f]);
			Ch(NumAlphabet::Upper[ c       & 0x0f]);

			text.DropByte();
		}

		return *this;
	}


	void Enc::ConsumeHtmlCharRefOrAmpersand(Seq& reader, Html::CharRefs charRefs)
	{
		EnsureThrow(reader.ReadByte() == '&');

		if (charRefs == Html::CharRefs::Escape)
			Add("&amp;");
		else
		{
			Seq readAhead = reader;
			bool found {};

			if (readAhead.StripPrefixExact("#"))
			{
				uint codePoint;

				if (readAhead.StripPrefixInsensitive("x"))
					codePoint = readAhead.ReadNrUInt32(16);
				else
					codePoint = readAhead.ReadNrUInt32Dec();

				if (readAhead.StripPrefixExact(";") && codePoint != UINT_MAX)
				{
					Add("&#").UInt(codePoint).Ch(';');
					found = true;
				}
			}
			else
			{
				Seq name = readAhead.ReadToFirstByteNotOfType(Ascii::IsAlpha);
				if (readAhead.StripPrefixExact(";"))
				{
					Html::CharRefNameInfo const* crni = Html::FindCharRefByName(name);
					if (crni)
					{
						Add("&#").UInt(crni->m_codePoint1).Ch(';');
						if (crni->m_codePoint2)
							Add("&#").UInt(crni->m_codePoint2).Ch(';');

						found = true;
					}
				}
			}

			if (found)
				reader = readAhead;
			else
				Add("&amp;");
		}
	}


	Enc& Enc::HtmlAttrValue(Seq value, Html::CharRefs charRefs)
	{
		// The attribute value MUST be quoted. Never use an unquoted attribute value. This function does NOT add the quotes.
		// & is escaped to prevent insertion of entities except known character references.
		// < must be escaped because it can't appear in XML attribute values.
		// " and ' are escaped to prevent ending attribute value.
		// ` must be escaped also because IE treats it as an alternative delimiter for attribute values.
		// US-ASCII control characters are escaped, including newlines and whitespace.
		auto isByteToEscape = [] (uint c) -> bool { return c=='&' || c=='<' || c=='"' || c=='\'' || c=='`' || c<=31 || c==127; };
		while (value.n)
		{
			Seq segment = value.ReadToFirstByteOfType(isByteToEscape);
			Add(segment);

			if (!value.n)
				break;

			uint c = value.FirstByte();
			if (c == '&')
				ConsumeHtmlCharRefOrAmpersand(value, charRefs);
			else
			{
				if (c == '<')
					Add("&lt;");
				else
					Add("&#").UInt(c).Ch(';');
				
				value.DropByte();
			}
		}

		return *this;
	}


	Enc& Enc::HtmlElemText(Seq text, Html::CharRefs charRefs)
	{
		// & is escaped to prevent insertion of entities except known character references.
		// < and > are escaped to prevent effects related to element tags.
		// US-ASCII control characters except \r\n\t are escaped.
		auto isByteToEscape = [] (uint c) -> bool { return c=='&' || c=='<' || c=='>' || (c<=31 && c!=13 && c!=10 && c!=9) || c==127; };
		while (text.n)
		{
			Seq segment = text.ReadToFirstByteOfType(isByteToEscape);
			Add(segment);

			if (!text.n)
				break;

			uint c = text.FirstByte();
			if (c == '&')
				ConsumeHtmlCharRefOrAmpersand(text, charRefs);
			else
			{
				switch (c)
				{
				case '<': Add("&lt;"); break;
				case '>': Add("&gt;"); break;
				default:  Add("&#").UInt(c).Ch(';'); break;
				}
				
				text.DropByte();
			}
		}

		return *this;
	}


	Enc& Enc::JsStrEncode(Seq text)
	{
		// This function is suitable for encoding JavaScript string content enclosed in " or '.
		// It is NOT suitable to encode ES6 template string literals enclosed in backticks!
		// & is not escaped: browsers do not parse HTML entities in script files and within <script> tags.
		// < is escaped to prevent embedding of "</script>" or "<!--".
		// \ is escaped because so it can be preserved (since it's a JS escape character).
		// " and ' are escaped to prevent ending the JS string.
		// US-ASCII control characters are escaped, including newlines and whitespace (other than space).
		// U+2028 and U+2029 are escaped because JavaScript treats them as line terminators.

		while (text.n)
		{
			uint c;
			EnsureThrow(Utf8::ReadCodePoint(text, c) == Utf8::ReadResult::OK);
			
			switch (c)
			{	
			case      8: Add("\\b"); break;
			case      9: Add("\\t"); break;
			case     10: Add("\\n"); break;
			case     11: Add("\\v"); break;
			case     12: Add("\\f"); break;
			case     13: Add("\\r"); break;
			case    '"': Add("\\\""); break;
			case   '\'': Add("\\'"); break;
			case   '\\': Add("\\\\"); break;
			default:
					 if (c <= 31 || c=='<' || c == 127) Add("\\x").UInt(c, 16, 2, CharCase::Lower);
				else if (c == 0x2028 || c == 0x2029)    Add("\\u").UInt(c, 16, 4, CharCase::Lower);
				else                                    Utf8Char(c);
				break;
			}
		}

		return *this;
	}


	Enc& Enc::CsvStrEncode(Seq text)
	{
		// As per RFC 4180, the only escaping of CSV escaped content (enclosed in double quotes) is to replace " with ""
		while (text.n)
		{
			Seq segment { text.ReadToByte('"') };
			Add(segment);

			if (!text.n)
				break;

			Add("\"\"");
			text.DropByte();
		}
		return *this;
	}


	Enc& Enc::CDataEncode(Seq text)
	{
		if (text.n)
		{
			Add("<![CDATA[");

			while (true)
			{
				Seq segment = text.ReadToString("]]>");
				Add(segment);

				if (!text.n)
					break;

				Add("]]]]><![CDATA[>");
				text.DropBytes(3);
			}

			Add("]]>");
		}
		return *this;
	}


	Enc& Enc::ImfCommentEncode(Seq text)
	{
		while (text.n)
		{
			Seq literalSegment = text.ReadToFirstUtf8CharNotOfType(Imf::Is_std_ctext_char);
			while (literalSegment.n)
			{
				// '&' and '#' are normal characters which are part of "ctext" and do not need escaping.
				// However, we use an HTML-like "&#...;" sequence to encode characters that we cannot escape using standard means.
				// Therefore, we avoid encoding the literal sequence "&#" if it appears in the source text, and replace it with "\&#".
				Seq subSegment = literalSegment.ReadToString("&#");
				Add(subSegment);

				if (!literalSegment.n)
					break;

				literalSegment.DropBytes(2);
				Add("\\&#");
			}

			if (!text.n)
				break;

			Seq escapeSegment = text.ReadToFirstUtf8CharOfType(Imf::Is_std_ctext_char);
			uint c;
			while (UINT_MAX != (c = escapeSegment.ReadUtf8Char()))
			{
				if (c >= 0x80 || !Imf::Is_quoted_pair_char(c))
					Add("&#").UInt(c).Ch(';');		// Use HTML-like escaping to preserve char value for human inspection
				else
				{
					Ch('\\');
					Byte((byte) c);
				}
			}
		}
		return *this;
	}

}
