#pragma once

#include "AtAscii.h"


namespace At
{
	struct UnicodeCharInfo { enum { NrPlanes=3, PartsPerPlane=8, TotalParts=NrPlanes*PartsPerPlane }; };
	extern char const c_unicodeCharInfo[UnicodeCharInfo::TotalParts][8193];

	namespace Unicode
	{
		enum
		{
			MaxAssignedCodePoint = 0x02FFFF,
			NrAssignedCodePoints = MaxAssignedCodePoint + 1,
			MaxPossibleCodePoint = 0x10FFFF,

			ReplacementChar = 0x00FFFD,
		};

		struct Category
		{
			enum
			{
				Unknown						=   0,

				Other_First                 =   4,
				Other_Control				=   4,
				Other_Format            	=   8,
				Other_NotAssigned       	=  12,
				Other_PrivateUse        	=  16,
				Other_Surrogate         	=  20,
				Other_Last                  =  20,

				Letter_First                =  24,
				Letter_Cased            	=  24,
				Letter_Lowercase        	=  28,
				Letter_Modifier         	=  32,
				Letter_Other            	=  36,
				Letter_Titlecase			=  40,
				Letter_Uppercase			=  44,
				Letter_Last                 =  44,

				Mark_First                  =  48,
				Mark_SpacingCombining		=  48,
				Mark_Enclosing				=  52,
				Mark_Nonspacing				=  56,
				Mark_Last                   =  56,

				Number_First                =  60,
				Number_DecimalDigit			=  60,
				Number_Letter				=  64,
				Number_Other				=  68,
				Number_Last                 =  68,

				Punctuation_First           =  72,
				Punctuation_Connector		=  72,
				Punctuation_Dash			=  76,
				Punctuation_Close			=  80,
				Punctuation_FinalQuote		=  84,
				Punctuation_InitialQuote	=  88,
				Punctuation_Other			=  92,
				Punctuation_Open			=  96,
				Punctuation_Last            =  96,

				Symbol_First                = 100,
				Symbol_Currency				= 100,
				Symbol_Modifier				= 104,
				Symbol_Math					= 108,
				Symbol_Other				= 112,
				Symbol_Last                 = 112,
				
				Separator_First             = 116,
				Separator_Line				= 116,
				Separator_Paragraph			= 120,
				Separator_Space				= 124,
				Separator_Last              = 124,

				Invalid						= 252,
			};

			static char const* ValueToName(uint v);
			static char const* ValueToCode(uint v);
			static uint CodeToValue(byte const* p, sizet n);		// Avoid #include "AtSeq.h" because this header is included by it
		};

		struct CharInfo
		{
			uint m_cat   {};
			uint m_width {};

			CharInfo() = default;
			CharInfo(uint cat, uint width) : m_cat(cat), m_width(width) {}

			static CharInfo Get(uint c) noexcept;
		};

		inline bool IsControl     (uint c) noexcept { return (c <= 0x1F) || (c >= 0x7F && c <= 0x9F); }
	
		inline bool IsSurrogate   (uint c) noexcept { return c >= 0xD800 && c <= 0xDFFF; }
		inline bool IsHiSurrogate (uint c) noexcept { return c >= 0xD800 && c <= 0xDBFF; }
		inline bool IsLoSurrogate (uint c) noexcept { return c >= 0xDC00 && c <= 0xDFFF; }

		uint JoinSurrogates(uint hi, uint lo);

		inline bool IsValidCodePoint(uint c) noexcept { return c <= MaxPossibleCodePoint && !IsSurrogate(c); }

		inline bool IsCategory(uint c, uint category) noexcept { return CharInfo::Get(c).m_cat == category; }
		inline bool IsCat_SepSpace(uint c) noexcept { return IsCategory(c, Category::Separator_Space); }
		inline bool IsWhitespace(uint c) noexcept { return Ascii::IsWhitespace(c) || (!Ascii::Is7Bit(c) && IsCat_SepSpace(c)); }
	};
}
