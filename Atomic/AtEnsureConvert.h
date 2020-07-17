#pragma once

#include "AtNum.h"


namespace At
{

	template <class CharType>
	struct EnsureConvertBase : NoCopy
	{
		~EnsureConvertBase() noexcept;
		CharType const* Z() const noexcept;

	protected:
		sizet           m_storageBytes {};
		CharType*       m_descBuf      {};
		CharType const* m_altDesc      {};

		bool AllocStorage(sizet storageChars) noexcept;
	};

		
	struct EnsureConvert_WtoA : EnsureConvertBase<char>
	{
		EnsureConvert_WtoA() noexcept {}
		EnsureConvert_WtoA(wchar_t const* descW, UINT cp) noexcept { Convert(descW, cp); }
		void Convert(wchar_t const* descW, UINT cp) noexcept;
	};

		
	struct EnsureConvert_AtoW : EnsureConvertBase<wchar_t>
	{
		EnsureConvert_AtoW() noexcept {}
		EnsureConvert_AtoW(char const* descA, UINT cp) noexcept { Convert(descA, cp); }
		void Convert(char const* descA, UINT cp) noexcept;
	};

}
