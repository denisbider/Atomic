#pragma once

#include "AtException.h"
#include "AtIncludes.h"
#include "AtStr.h"


namespace At
{

	void DescribeWinErr(Enc& s, int64 err);
	void DescribeNtStatus(Enc& s, int64 st);

	char const* GetPreWin32FileOpErrDesc(int64 err);	// Returns nullptr if not a pre-Win32 SHFileOperation error


	template <class T = StrErr>
	struct WinErr : public ErrWithCode<T>
	{
		WinErr(int64 code, Seq desc) : ErrWithCode<T>(code, desc) {}
		void BuildDesc(Str& s) const override { s.Add(": ").Fun(DescribeWinErr, m_code); }
	};

	template <class T = StrErr>
	struct NtStatusErr : public ErrWithCode<T>
	{
		NtStatusErr(int64 code, Seq desc) : ErrWithCode<T>(code, desc) {}
		void BuildDesc(Str& s) const override { s.Add(": ").Fun(DescribeNtStatus, m_code); }
	};

	template <class T = StrErr>
	struct WinFileOpErr : public ErrWithCode<T>
	{
		WinFileOpErr(int64 code, Seq desc) : ErrWithCode<T>(code, desc) {}
		void BuildDesc(Str& s) const override
		{
			char const* desc = GetPreWin32FileOpErrDesc(m_code);
			s.Add(": ");
			if (desc)
				s.Add("Pre-Win32 error ").ErrCode(m_code).Add(": ").Add(desc);
			else
				s.Fun(DescribeWinErr, m_code);
		}
	};

	struct LastWinErr
	{
		// CORRECT: { LastWinErr e; throw e.Make<>(Str("Error during ").Add(m_operation)); }
		// INCORRECT: throw LastWinErr<>(Str("Error during ").Add(m_operation));
		//   The dynamic string is definitely constructed before the exception constructor is invoked, so GetLastError() can and will return incorrect result.
		// INCORRECT: throw LastWinErr().Make<>(Str("Error during ").Add(m_operation));
		//   Since C++ order of evaluation is unspecified, the compiler can and will delay construction of temporary until after construction of dynamic string.
		LastWinErr() : m_err(GetLastError()) {}
		LastWinErr(LastWinErr const&) = delete;		// Prevent direct throwing of this type as an exception

		template <class T = StrErr>
		WinErr<T> Make(Seq desc) const { return WinErr<T>(m_err, desc); }

		DWORD const m_err;
	};

	struct LastWsaErr
	{
		// CORRECT: { LastWsaErr e; throw e.Make<>(Str("Error during ").Add(m_operation)); }
		// INCORRECT: throw LastWsaErr<>(Str("Error during ").Add(m_operation));
		//   The dynamic string is definitely constructed before the exception constructor is invoked, so WSAGetLastError() can and will return incorrect result.
		// INCORRECT: throw  LastWsaErr().Make<>(Str("Error during ").Add(m_operation));
		//   Since C++ order of evaluation is unspecified, the compiler can and will delay construction of temporary until after construction of dynamic string.
		LastWsaErr() : m_err(WSAGetLastError()) {}
		LastWsaErr(LastWsaErr const&) = delete;		// Prevent direct throwing of this type as an exception

		template <class T = StrErr>
		WinErr<T> Make(Seq desc) const { return WinErr<T>(m_err, desc); }

		int const m_err;
	};

}
