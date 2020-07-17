#include "AtIncludes.h"
#include "AtJsonReadWrite.h"


namespace At
{
	namespace Json
	{

		// Encode

		void EncodeStringPart(Enc& enc, Seq s)
		{
			while (s.n)
			{
				uint c;
				EnsureThrow(Utf8::ReadCodePoint(s, c) == Utf8::ReadResult::OK);

				switch (c)
				{	
				case    8: enc.Add("\\b"); break;
				case    9: enc.Add("\\t"); break;
				case   10: enc.Add("\\n"); break;
				case   12: enc.Add("\\f"); break;
				case   13: enc.Add("\\r"); break;
				case  '"': enc.Add("\\\""); break;
				case '\\': enc.Add("\\\\"); break;
				default:
					// Escape < to prevent "</script" or "<!--" in case the JSON is embedded in a <script> tag
					// Escape Unicode line terminators that are valid in JSON, but not in JavaScript
					if (c <= 31 || c=='<' || c == 127 || c == 0x2028 || c == 0x2029)
						enc.Add("\\u").UInt(c, 16, 4, CharCase::Lower);
					else
						enc.Utf8Char(c);
					break;
				}
			}
		}


		void EncodeTime(Enc& enc, Time t)
		{
			if (!t.Any())
				enc.Add("0");
			else
				enc.Ch('"').Obj(t, TimeFmt::IsoMicroZ, 'T').Ch('"');
		}


		void EncodeArray(Enc& enc, sizet nrValues, std::function<void(Enc&, sizet)> encodeValue)
		{
			enc.Ch('['); 

			for (sizet i=0; i!=nrValues; ++i)
			{
				if (i > 0)
					enc.Ch(',');

				encodeValue(enc, i);
			}

			enc.Ch(']');
		}



		// Decode

		DecodeErr::DecodeErr(ParseNode const& p, Seq s)
			: StrErr(Str("JSON decode error at row ").UInt(p.StartRow()).Add(", col ").UInt(p.StartCol()).Add(": ").Add(s))
		{
		}


		uint DecodeEscUnicode(ParseNode const& p)
		{
			if (!p.IsType(id_EscUnicode))
				throw DecodeErr(p, "Expected Unicode escape sequence");

			Seq s = p.SrcText().DropBytes(2);
			EnsureThrow(s.n == 4);
			return s.ReadNrUInt16(16);
		}


		void DecodeString(ParseNode const& p, Enc& enc)
		{
			if (!p.IsType(id_String))
				throw DecodeErr(p, "Expected string");

			enc.ReserveInc(p.SrcText().n);
			
			uint leadSurrogate = 0;
			bool needTrailSurrogate = false;

			for (ParseNode const& e : p)
			{
				if (needTrailSurrogate)
				{
					if (!e.IsType(id_EscUnicode))
						throw DecodeErr(e, "Expected Unicode escape sequence with trail surrogate in surrogate pair");

					uint trailSurrogate = DecodeEscUnicode(e);
					if (trailSurrogate < 0xDC00 || trailSurrogate > 0xDFFF)
						throw DecodeErr(e, "Expected trail surrogate in surrogate pair");

					uint c = 0x010000 + (((leadSurrogate & 0x03FF) << 10) | (trailSurrogate & 0x03FF));
					enc.Utf8Char(c);

					needTrailSurrogate = false;
				}
				else
				{
					if (e.IsType(id_CharsNonEsc))
					{
						enc.Add(e.SrcText());
					}
					else if (e.IsType(id_EscSpecial))
					{
						EnsureThrow(e.SrcText().n >= 2);
						byte c = e.SrcText().p[1];

						switch (c)
						{
						case '"' :	enc.Ch('"');  break;
						case '\\':	enc.Ch('\\'); break;
						case '/' :	enc.Ch('/');  break;
						case 'b' :	enc.Byte( 8); break;
						case 't' :	enc.Byte( 9); break;
						case 'n' :	enc.Byte(10); break;
						case 'f' :	enc.Byte(12); break;
						case 'r' :	enc.Byte(13); break;
						default:	throw DecodeErr(e, "Unrecognized string escape sequence");
						}
					}
					else if (e.IsType(id_EscUnicode))
					{
						uint c = DecodeEscUnicode(e);
						if (c >= 0xD800 && c <= 0xDBFF)
						{
							leadSurrogate = c;
							needTrailSurrogate = true;
						}
						else if (c >= 0xDC00 && c <= 0xDFFF)
						{
							throw DecodeErr(e, "Unexpected trail surrogate");
						}
						else
							enc.Utf8Char(c);
					}
					else if (!e.IsType(Parse::id_DblQuote))
						throw DecodeErr(e, "Unrecognized string element type");
				}
			}

			if (needTrailSurrogate)
				throw DecodeErr(p.LastChild(), "String ends with an unterminated surrogate pair");
		}


		bool DecodeBool(ParseNode const& p)
		{
				if (!p.IsType(Json::id_True) && !p.IsType(Json::id_False))
					throw Json::DecodeErr(p, "Expected true or false");
				return (p.IsType(Json::id_True));
		}


		uint64 DecodeUInt(ParseNode const& p)
		{
			if (!p.IsType(Json::id_Number))
				throw Json::DecodeErr(p, "Expected number");

			Seq    reader { p.SrcText() };
			uint64 v      { reader.ReadNrUInt64Dec() };
			if (reader.n != 0)
			{
				reader = p.SrcText();
				double dv = reader.ReadDouble();
				if (reader.n != 0)
					throw Json::DecodeErr(p, "Unexpected number format");
				if (dv < 0 || dv > (double) UINT64_MAX)
					throw Json::DecodeErr(p, "Expected unsigned number out of range");

				v = (uint64) (dv + 0.5);
			}

			return v;
		}


		int64 DecodeSInt(ParseNode const& p)
		{
			if (!p.IsType(Json::id_Number))
				throw Json::DecodeErr(p, "Expected number");

			Seq   reader { p.SrcText() };
			int64 v      { reader.ReadNrSInt64Dec() };
			if (reader.n != 0)
			{
				reader = p.SrcText();
				double dv = reader.ReadDouble();
				if (reader.n != 0)
					throw Json::DecodeErr(p, "Unexpected number format");
				if (dv < (double) INT64_MIN || dv > (double) INT64_MAX)
					throw Json::DecodeErr(p, "Expected signed number out of range");

				v = (int64) If(dv >= 0.0, double, (dv + 0.5), (dv - 0.5));
			}

			return v;
		}


		double DecodeFloat(ParseNode const& p)
		{
			if (!p.IsType(Json::id_Number))
				throw Json::DecodeErr(p, "Expected number");

			Seq    reader { p.SrcText() };
			double dv     { reader.ReadDouble() };
			if (reader.n != 0)
				throw Json::DecodeErr(p, "Unexpected number format");

			return dv;
		}


		Time DecodeTime(ParseNode const& p)
		{
			Time v;

			if (p.IsType(Json::id_Number))
			{
				if (!p.SrcText().EqualExact("0"))
					throw Json::DecodeErr(p, "Date-time is encoded as a number and it is not 0");
			}
			else
			{
				if (!p.IsType(Json::id_String))
					throw Json::DecodeErr(p, "Expected 0 or string containing ISO 8601-style UTC date or date-time");

				Str s;
				Json::DecodeString(p, s);

				Seq reader { s };
				if (!reader.ReadIsoStyleTimeStr(v) || !reader.EqualExact("Z"))
					throw Json::DecodeErr(p, "Expected ISO 8601-style UTC date or date-time");
			}

			return v;
		}


		void DecodeArray(ParseNode const& p, std::function<void (ParseNode const&)> decodeValue)
		{
			if (!p.IsType(id_Array))
				throw DecodeErr(p, "Expected array");

			for (ParseNode const& c : p)
				if (!c.IsType(Parse::id_SqOpenBr) &&
					!c.IsType(Parse::id_SqCloseBr) &&
					!c.IsType(Parse::id_Ws) &&
					!c.IsType(Parse::id_Comma))
				{
					decodeValue(c);
				}
		}


		void DecodeObject(ParseNode const& p, std::function<bool (ParseNode const&, Seq, ParseNode const&)> decodePair)
		{
			if (!p.IsType(Json::id_Object))
				throw Json::DecodeErr(p, "Expected object");

			for (ParseNode const& c : p)
				if (c.IsType(Json::id_Pair))
				{
					ParseNode const& n = c.FirstChild();
					EnsureThrow(n.IsType(Json::id_String));

					Str name;
					Json::DecodeString(n, name);

					if (!decodePair(n, name, c.LastChild()))
						break;
				}
		}

	}
}
