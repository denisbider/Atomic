#include "AtIncludes.h"
#include "AtUnicode.h"

#include "AtEnsure.h"
#include "AtSeq.h"


namespace At
{
	namespace Unicode
	{

		char const* Category::ValueToName(uint v)
		{
			switch (v)
			{
			case Invalid:					return "(Invalid)";
			case Unknown:					return "(Unknown)";
			case Other_Control:				return "Other_Control";
			case Other_Format:				return "Other_Format";
			case Other_NotAssigned:			return "Other_NotAssigned";
			case Other_PrivateUse:			return "Other_PrivateUse";
			case Other_Surrogate:			return "Other_Surrogate";
			case Letter_Cased:				return "Letter_Cased";
			case Letter_Lowercase:			return "Letter_Lowercase";
			case Letter_Modifier:			return "Letter_Modifier";
			case Letter_Other:				return "Letter_Other";
			case Letter_Titlecase:			return "Letter_Titlecase";
			case Letter_Uppercase:			return "Letter_Uppercase";
			case Mark_SpacingCombining:		return "Mark_SpacingCombining";
			case Mark_Enclosing:			return "Mark_Enclosing";
			case Mark_Nonspacing:			return "Mark_Nonspacing";
			case Number_DecimalDigit:		return "Number_DecimalDigit";
			case Number_Letter:				return "Number_Letter";
			case Number_Other:				return "Number_Other";
			case Punctuation_Connector:		return "Punctuation_Connector";
			case Punctuation_Dash:			return "Punctuation_Dash";
			case Punctuation_Close:			return "Punctuation_Close";
			case Punctuation_FinalQuote:	return "Punctuation_FinalQuote";
			case Punctuation_InitialQuote:	return "Punctuation_InitialQuote";
			case Punctuation_Other:			return "Punctuation_Other";
			case Punctuation_Open:			return "Punctuation_Open";
			case Symbol_Currency:			return "Symbol_Currency";
			case Symbol_Modifier:			return "Symbol_Modifier";
			case Symbol_Math:				return "Symbol_Math";
			case Symbol_Other:				return "Symbol_Other";
			case Separator_Line:			return "Separator_Line";
			case Separator_Paragraph:		return "Separator_Paragraph";
			case Separator_Space:			return "Separator_Space";
			default:						return "(Unrecognized)";
			}
		}


		char const* Category::ValueToCode(uint v)
		{
			switch (v)
			{
			case Invalid:					return "(Invalid)";
			case Unknown:					return "(Unknown)";
			case Other_Control:				return "Cc";
			case Other_Format:				return "Cf";
			case Other_NotAssigned:			return "Cn";
			case Other_PrivateUse:			return "Co";
			case Other_Surrogate:			return "Cs";
			case Letter_Cased:				return "LC";
			case Letter_Lowercase:			return "Ll";
			case Letter_Modifier:			return "Lm";
			case Letter_Other:				return "Lo";
			case Letter_Titlecase:			return "Lt";
			case Letter_Uppercase:			return "Lu";
			case Mark_SpacingCombining:		return "Mc";
			case Mark_Enclosing:			return "Me";
			case Mark_Nonspacing:			return "Mn";
			case Number_DecimalDigit:		return "Nd";
			case Number_Letter:				return "Nl";
			case Number_Other:				return "No";
			case Punctuation_Connector:		return "Pc";
			case Punctuation_Dash:			return "Pd";
			case Punctuation_Close:			return "Pe";
			case Punctuation_FinalQuote:	return "Pf";
			case Punctuation_InitialQuote:	return "Pi";
			case Punctuation_Other:			return "Po";
			case Punctuation_Open:			return "Ps";
			case Symbol_Currency:			return "Sc";
			case Symbol_Modifier:			return "Sk";
			case Symbol_Math:				return "Sm";
			case Symbol_Other:				return "So";
			case Separator_Line:			return "Zl";
			case Separator_Paragraph:		return "Zp";
			case Separator_Space:			return "Zs";
			default:						return "(Unrecognized)";
			}
		}


		uint Category::CodeToValue(byte const* p, sizet n)
		{
			if (n != 2) return Invalid;
			Seq c { p, n };
			if (c == "Cc") return Other_Control;
			if (c == "Cf") return Other_Format;
			if (c == "Cn") return Other_NotAssigned;
			if (c == "Co") return Other_PrivateUse;
			if (c == "Cs") return Other_Surrogate;
			if (c == "LC") return Letter_Cased;
			if (c == "Ll") return Letter_Lowercase;
			if (c == "Lm") return Letter_Modifier;
			if (c == "Lo") return Letter_Other;
			if (c == "Lt") return Letter_Titlecase;
			if (c == "Lu") return Letter_Uppercase;
			if (c == "Mc") return Mark_SpacingCombining;
			if (c == "Me") return Mark_Enclosing;
			if (c == "Mn") return Mark_Nonspacing;
			if (c == "Nd") return Number_DecimalDigit;
			if (c == "Nl") return Number_Letter;
			if (c == "No") return Number_Other;
			if (c == "Pc") return Punctuation_Connector;
			if (c == "Pd") return Punctuation_Dash;
			if (c == "Pe") return Punctuation_Close;
			if (c == "Pf") return Punctuation_FinalQuote;
			if (c == "Pi") return Punctuation_InitialQuote;
			if (c == "Po") return Punctuation_Other;
			if (c == "Ps") return Punctuation_Open;
			if (c == "Sc") return Symbol_Currency;
			if (c == "Sk") return Symbol_Modifier;
			if (c == "Sm") return Symbol_Math;
			if (c == "So") return Symbol_Other;
			if (c == "Zl") return Separator_Line;
			if (c == "Zp") return Separator_Paragraph;
			if (c == "Zs") return Separator_Space;
			return Unknown;
		}


		CharInfo CharInfo::Get(uint c) noexcept
		{
			if (c > MaxPossibleCodePoint)
				return CharInfo(Unicode::Category::Invalid, 0);

			uint partNr { c >> 13 };
			if (partNr >= UnicodeCharInfo::TotalParts)
				return CharInfo(Unicode::Category::Unknown, 0);

			uint v { (unsigned char) (c_unicodeCharInfo[partNr][c & 0x1FFF]) };
			return CharInfo(v & 0xFC, v & 0x03);
		}


		uint JoinSurrogates(uint hi, uint lo)
		{
			EnsureThrow(IsHiSurrogate(hi));
			EnsureThrow(IsLoSurrogate(lo));

			return 0x10000 + (((hi - 0xD800) << 10) | (lo - 0xDC00));
		}
	}
}
