#include "AtIncludes.h"
#include "AtEnsureConvert.h"


namespace At
{

	// EnsureConvertBase

	template <class CharType>
	EnsureConvertBase<CharType>::~EnsureConvertBase() noexcept
	{
		if (m_descBuf)
			VirtualFree(m_descBuf, m_storageBytes, MEM_RELEASE);
	}


	template <class CharType>
	bool EnsureConvertBase<CharType>::AllocStorage(sizet storageChars) noexcept
	{
		sizet bytesNeeded = storageChars * sizeof(CharType);
		if (m_storageBytes >= bytesNeeded)
			return true;

		if (m_descBuf)
			VirtualFree(m_descBuf, m_storageBytes, MEM_RELEASE);

		m_storageBytes = ((bytesNeeded / 4096) + 1) * 4096;
		m_descBuf = (CharType*) VirtualAlloc(nullptr, m_storageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (m_descBuf)
			return true;

		m_storageBytes = 0;
		return false;
	}


	template <class CharType>
	inline CharType const* EnsureConvertBase<CharType>::Z() const noexcept
	{
		if (m_altDesc) return m_altDesc;
		return m_descBuf;
	}


	template struct EnsureConvertBase<char>;
	template struct EnsureConvertBase<wchar_t>;

		

	// EnsureConvert_WtoA

	void EnsureConvert_WtoA::Convert(wchar_t const* descW, UINT cp) noexcept
	{
		sizet inLen = wcslen(descW);
		sizet storageChars = PickMax<sizet>(2U*inLen, 1000U);
		if (!AllocStorage(storageChars))
			m_altDesc = "Ensure check failed. Additionally, could not allocate memory for failure description.";
		else
		{
			int len = WideCharToMultiByte(cp, 0, descW, -1, m_descBuf, SatCast<int>(storageChars), 0, 0);
			if (len <= 0)
				m_altDesc = "Ensure check failed. Additionally, an error occurred converting the failure description from UCS-2.";
		}
	}

		
	// EnsureConvert_AtoW

	void EnsureConvert_AtoW::Convert(char const* descA, UINT cp) noexcept
	{
		sizet inLen = strlen(descA);
		sizet storageChars = PickMax<sizet>(2U*inLen, 1000U);
		if (!AllocStorage(storageChars))
			m_altDesc = L"Ensure check failed. Additionally, could not allocate memory for failure description.";
		else
		{
			int len = MultiByteToWideChar(cp, 0, descA, -1, m_descBuf, SatCast<int>(storageChars));
			if (len <= 0)
				m_altDesc = L"Ensure check failed. Additionally, an error occurred converting the failure description to UCS-2.";
		}
	}

}
