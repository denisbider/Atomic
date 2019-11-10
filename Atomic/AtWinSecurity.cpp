#include "AtIncludes.h"
#include "AtWinSecurity.h"

#include "AtWinErr.h"

namespace At
{
	// SecDescriptor

	SecDescriptor::~SecDescriptor()
	{
		Clear();
	}

	SecDescriptor& SecDescriptor::Clear()
	{
		if (m_psd != nullptr)
		{
			LocalFree(m_psd);
			m_psd = nullptr;
		}
		return *this;
	}

	SecDescriptor& SecDescriptor::Set(wchar_t const* sddlStr)
	{
		EnsureThrow(sddlStr != nullptr);

		Clear();

		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddlStr, SDDL_REVISION_1, &m_psd, nullptr))
			{ LastWinErr e; throw e.Make<>("SecDescriptor: Error in ConvertStringSecurityDescriptorToSecurityDescriptor"); }

		return *this;
	}

	// TokenInfo

	void TokenInfo::GetInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS infoClass, sizet firstCallBufSize)
	{
		if (firstCallBufSize != 0)
			m_buf.Resize(firstCallBufSize);

		enum { MaxTryCount = 5 };
		sizet tryNr = 0;
		while (true)
		{
			++tryNr;

			DWORD errorCode = 0;
			DWORD retLen = 0;
			if (GetTokenInformation(hToken, infoClass, m_buf.Ptr(), NumCast<DWORD>(m_buf.Len()), &retLen))
			{
				m_buf.Resize(retLen);
				break;
			}

			errorCode = GetLastError();
			if (tryNr >= MaxTryCount || errorCode != ERROR_INSUFFICIENT_BUFFER)
				throw WinErr<>(errorCode, "GetTokenInfo: Error in GetTokenInformation");

			m_buf.Resize(retLen * tryNr);
		}
	}
}
