#include "AtIncludes.h"
#include "AtMimeQuotedPrintable.h"

namespace At
{
	namespace Mime
	{
		// Quoted-Printable encoding and decoding implemented according to RFC 2045, section 6.7

		Seq QuotedPrintableEncode(Seq plain, Enc& encoded)
		{
			Enc::Meter meter = encoded.IncMeter(QuotedPrintableEncode_MaxLen(plain.n));

			sizet lineLen = 0;
			while (plain.n != 0)
			{
				if (plain.StartsWithExact("\r\n"))
				{
					// CRLF appears in source text, represent it with CRLF
					encoded.Add("\r\n");
					plain.DropBytes(2);
					lineLen = 0;
				}
				else
				{
					uint c         = plain.ReadByte();
					bool endOfLine = (!plain.n || plain.StartsWithExact("\r\n"));
					bool verbatim  = ((c == 32 && !endOfLine)	||		// Skip !"#$ (33 -36). Also, space must be escaped at end of line
									  (c >= 37 && c <= 60)		||		// Skip =    (61)
									  (c >= 62 && c <= 63)		||		// Skip @    (64)
									  (c >= 65 && c <= 90)		||      // Skip [\]^ (91 - 94)
									  (c == 95)					||      // Skip `    (96)
									  (c >= 97 && c <= 122));			// Skip {|}~ (123 - 126)

					sizet newLen = lineLen + (verbatim ? 1 : 3);
					sizet maxLen = (sizet) (endOfLine ? 76 : 75);
					if (newLen > maxLen)
					{
						encoded.Add("=\r\n");
						lineLen = 0;
					}

					if (verbatim)
					{
						encoded.Byte((byte) c);
						++lineLen;
					}
					else
					{
						encoded.Ch('=').Hex(c);
						lineLen += 3;
					}
				}
			}

			return meter.WrittenSeq();
		}


		Seq QuotedPrintableDecode(Seq& encoded, Enc& decoded)
		{
			Enc::Meter meter = decoded.IncMeter(encoded.n);

			while (true)
			{
				Seq chunk = encoded.ReadToFirstByteOf("=\r\n");
				if (encoded.StartsWithExact("="))
				{
					decoded.Add(chunk);
					encoded.DropByte();
					if (!encoded.n)
						break;
	
					uint h = encoded.ReadHexEncodedByte();
					if (h != UINT_MAX)
						decoded.Byte((byte) h);
					else
					{
						// If equal sign is not followed by a hex char, it must be followed by optional whitespace, and then end of line / end of input
						Seq ws = encoded.ReadToFirstByteNotOf(" \t");
						if (!encoded.n)
							break;								// We're at end of input

						if (encoded.StartsWithExact("\r\n"))
							encoded.DropBytes(2);
						else if (encoded.StartsWithExact("\n"))	// Permit newline conversion to have occurred
							encoded.DropByte();
						else
							decoded.Byte('=').Add(ws);			// Invalid escape sequence - preserve verbatim
					}
				}
				else
				{
					// We are either at end of line, or end of input
					decoded.Add(chunk.TrimRight());				// Trim trailing white space, if any
					if (!encoded.n)
						break;									// We're at end of input

					if (encoded.StartsWithExact("\r\n"))
						encoded.DropBytes(2);
					else if (encoded.StartsWithExact("\n"))		// Permit newline conversion to have occurred, but still interpret it to represent CRLF in source text
						encoded.DropByte();
					else
						EnsureThrow(!"Unexpected quoted-printable decoding state");

					decoded.Add("\r\n");
				}
			}

			return meter.WrittenSeq();
		}

	}
}
