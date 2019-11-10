#pragma once

#include "AtStr.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	// SecDescriptor
	
	class SecDescriptor : NoCopy
	{
	public:
		SecDescriptor() {}
		SecDescriptor(Seq sddlStr) { Set(sddlStr); }
		SecDescriptor(wchar_t const* sddlStr) { Set(sddlStr); }

		~SecDescriptor();

		SecDescriptor& Clear();

		SecDescriptor& Set(Seq sddlStr) { return Set(WinStr(sddlStr).Z()); }
		SecDescriptor& Set(wchar_t const* sddlStr);

		PSECURITY_DESCRIPTOR Ptr() { return m_psd; }

	private:
		PSECURITY_DESCRIPTOR m_psd {};
	};


	// SecAttrs

	struct InheritHandle { enum E { No = FALSE, Yes = TRUE }; };

	class SecAttrs : NoCopy
	{
	public:
		SecAttrs() { Clear(); }
		SecAttrs(SecDescriptor& sd, InheritHandle::E ih) { Set(sd, ih); }

		SecAttrs& Clear() { ZeroMemory(&m_sa, sizeof(m_sa)); m_sa.nLength = sizeof(m_sa); return *this; }
		SecAttrs& Set(SecDescriptor& sd, InheritHandle::E ih) { Clear(); m_sa.lpSecurityDescriptor = sd.Ptr(); m_sa.bInheritHandle = ih; return *this; } 

		PSECURITY_ATTRIBUTES Ptr() { return &m_sa; }

	private:
		SECURITY_ATTRIBUTES m_sa;
	};


	// TokenInfo

	class TokenInfo : NoCopy
	{
	protected:
		void GetInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS infoClass, sizet firstCallBufSize);
		void const* VoidPtr() const { return m_buf.Ptr(); }
	
	public:
		sizet Len() const { return m_buf.Len(); }

	private:
		Str m_buf;
	};

	template <class T, TOKEN_INFORMATION_CLASS C>
	struct TokenInfoStatic : public TokenInfo
	{
		enum { InfoClass = C };
		void GetInfo(HANDLE hToken) { TokenInfo::GetInfo(hToken, C, sizeof(T)); }
		T const* Ptr() const { return (T const*) VoidPtr(); }
	};

	template <class T, TOKEN_INFORMATION_CLASS C>
	struct TokenInfoDynamic : public TokenInfo
	{
		enum { InfoClass = C };
		void GetInfo(HANDLE hToken) { TokenInfo::GetInfo(hToken, C, 0); }
		T const* Ptr() const { return (T const*) VoidPtr(); }
	};

	typedef TokenInfoDynamic<TOKEN_OWNER, TokenOwner> TokenInfo_Owner;

	template <class T>
	struct TokenInfoOfProcess : public T
	{
		TokenInfoOfProcess(HANDLE hProcess)
		{
			if (!OpenProcessToken(hProcess, InfoClass == TokenSource ? TOKEN_QUERY_SOURCE : TOKEN_QUERY, &m_hToken))
				{ LastWinErr e; throw e.Make<>("TokenInfoOfProcess: Error in OpenProcessToken"); }

			GetInfo(m_hToken);
		}

		~TokenInfoOfProcess() { CloseHandle(m_hToken); }

	private:
		HANDLE m_hToken;
	};


	// StringSid

	struct StringSid : NoCopy
	{
		StringSid(PSID sid)
		{
			if (!ConvertSidToStringSidA(sid, &m_stringSid))
				{ LastWinErr e; throw e.Make<>("StringSid: Error in ConvertSidToStringSid"); }
		}

		~StringSid() { LocalFree(m_stringSid); }

		LPCSTR GetStr() const { return m_stringSid; }

	private:
		LPSTR m_stringSid;
	};

}
