#pragma once

#include "AtSeq.h"


namespace At
{

	class Enc;

	struct BaseXY
	{
		enum class Padding { No, Yes };

		struct NewLines
		{
			NewLines(sizet maxCharsPerLine=SIZE_MAX, sizet initLineWidth=0, Seq newLine="\r\n")
				: m_maxCharsPerLine(maxCharsPerLine)
				, m_initLineWidth(initLineWidth)
				, m_nextNewLineWidth(Seq(newLine).DropToFirstByteNotOf("\r\n").n)
				, m_newLine(newLine) {}

			static NewLines None() { return NewLines();   }
			static NewLines Mime() { return NewLines(76); }

			// Given a base output length, calculates a ceiling for total output length after adding newlines
			sizet MaxOutLen(sizet baseOutLen) const { return baseOutLen + (m_newLine.n * (baseOutLen / m_maxCharsPerLine)); }

			sizet m_maxCharsPerLine;
			sizet m_initLineWidth;
			sizet m_nextNewLineWidth;
			Seq   m_newLine;
		};

		class Encoder
		{
		public:
			Encoder(byte* pWrite, NewLines const& nl) : m_pWrite(pWrite), m_nl(nl)
			{
				m_lineWidth = nl.m_initLineWidth;
			}

			void Add(byte c)
			{
				if (m_lineWidth++ > m_nl.m_maxCharsPerLine)
					AddNewLine();
				*m_pWrite++ = c;
			}

			byte* Done() { return m_pWrite; }

		private:
			NewLines const& m_nl;
			byte*           m_pWrite     {};
			sizet           m_lineWidth  {};

			__declspec(noinline) void AddNewLine()
			{
				for (byte b : m_nl.m_newLine)
					*m_pWrite++ = b;
				m_lineWidth = m_nl.m_nextNewLineWidth + 1;
			}
		};
	};


	struct Base64 : BaseXY
	{
		static char const* const BaseAlphabet;

		// The last3 parameter controls the last three characters of the Base64 alphabet that will be used.
		// If padding is true, trailing '=' characters will be appended instead of encoded data ending short.
		// The destination string is not cleared. Encoded data is appended to the string.

		static sizet EncodeBaseLen(sizet n) { return ((n / 3) + 1) * 4; }
		static sizet EncodeMaxOutLen(sizet n, NewLines const& nl) { return nl.MaxOutLen(EncodeBaseLen(n)); }
		static byte* Encode(Seq plain, byte* pWrite, Padding padding, char const* last3, NewLines const& nl);
		static Seq   Encode(Seq plain, Enc&  out,    Padding padding, char const* last3, NewLines const& nl);
		static inline Seq MimeEncode       (Seq plain, Enc& out, Padding padding, NewLines const& nl) { return Encode(plain, out, padding, "+/=", nl         ); }
		static inline Seq UrlFriendlyEncode(Seq plain, Enc& out, Padding padding)                     { return Encode(plain, out, padding, "-_.", NewLines() ); }


		// The destination buffer is not cleared. Decoded data is appended.
		// Skips whitespace. Stops decoding if any non-whitespace characters not in the alphabet are encountered.
		// Returns a Seq pointing to the data decoded.

		static sizet DecodeMaxLen(sizet n) { return ((n / 4) + 1) * 3; }
		static byte* Decode(Seq& encoded, byte* pWrite, char const* last3);
		static Seq   Decode(Seq& encoded, Enc&  out,    char const* last3);
		static inline byte* MimeDecode       (Seq& encoded, byte* pWrite ) { return Decode(encoded, pWrite, "+/="); }
		static inline Seq   MimeDecode       (Seq& encoded, Enc&  out    ) { return Decode(encoded, out,    "+/="); }
		static inline Seq   UrlFriendlyDecode(Seq& encoded, Enc&  out    ) { return Decode(encoded, out,    "-_."); }
	};


	struct Base32 : BaseXY
	{
		// We use a scheme similar to Crockford's (iI => 1, oO => 0, sS => 5), but we make use of the uppercase L when encoding.
		// This is so we can treat zZ as aliases for 2 when decoding. The lowercase l is still treated as an alias for the digit 1.
		// For letters where uppercase and lowercase are clearly distinct (abdeghnqr), lowercase is treated as invalid input.

		static byte const* const EncodeAlphabet;	// 32 digits and letters, 0-9 and A-Z, some letters excluded
		static byte const* const DecodeTable;		// Starts with decode value for '0' (ASCII 48) at index 0, and continues from there until the decode value for ASCII 127

		static sizet EncodeBaseLen(sizet n) { return ((n / 5) + 1) * 8; }
		static byte* Encode(Seq plain, byte* pWrite, NewLines const& nl);
		static Seq   Encode(Seq plain, Enc&  out,    NewLines const& nl);
		
		static sizet DecodeMaxLen(sizet n) { return ((n / 8) + 1) * 5; }
		static byte* Decode(Seq& encoded, byte* pWrite);
		static Seq   Decode(Seq& encoded, Enc&  out);
	};

}
