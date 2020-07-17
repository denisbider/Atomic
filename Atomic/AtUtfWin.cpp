#include "AtIncludes.h"
#include "AtUtfWin.h"

#include "AtDllNormaliz.h"
#include "AtNumCvt.h"
#include "AtUtf8.h"
#include "AtWinErr.h"


namespace At
{

	void ToUtf16(Seq in, Vec<wchar_t>& out, UINT inCodePage, NullTerm::E nt)
	{
		if (!in.n)
		{
			if (nt == NullTerm::No)
				out.Clear();
			else
			{
				out.ResizeExact(1);
				out[0] = 0;
			}
		}
		else
		{
			int cbMultiByte { NumCast<int>(in.n) };
			int rc { MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, cbMultiByte, 0, 0) };
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<>("ToUtf16: Error in first call to MultiByteToWideChar"); }

			int cchWideChar { rc + 4 };
			out.ResizeExact(NumCast<sizet>(cchWideChar));
			rc = MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, cbMultiByte, out.Ptr(), cchWideChar);
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<>("ToUtf16: Error in second call to MultiByteToWideChar"); }

			sizet outSize { NumCast<sizet>(rc) };
			if (nt == NullTerm::No)
				out.ResizeExact(outSize);
			else
			{
				out.ResizeExact(outSize + 1);
				out[outSize] = 0;
			}
		}
	}


	void NormalizeUtf16(PCWSTR pStr, int nWideChars, Vec<wchar_t>& out)
	{
		if (!nWideChars)
			out.Clear();
		else
		{
			int rc = Call_NormalizeString(NormalizationC, pStr, nWideChars, 0, 0);
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("NormalizeUtf16: Error in first call to NormalizeString"); }

			out.ResizeExact((sizet) rc);

			rc = Call_NormalizeString(NormalizationC, pStr, nWideChars, out.Ptr(), rc);
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("NormalizeUtf16: Error in second call to NormalizeString"); }

			// nWideChars may be -1 to indicate pStr is null-terminated.
			// In this case, NormalizeString will include and count the null terminator in its output.
			// We remove the null terminator here.
			if (nWideChars < 0 && rc > 0)
				--rc;

			out.ResizeExact((sizet) rc);
		}
	}


	void FromUtf16(PCWSTR pStr, int nWideChars, Enc& enc, UINT outCodePage)
	{
		if (nWideChars)
		{
			int rc { WideCharToMultiByte(outCodePage, 0, pStr, nWideChars, 0, 0, 0, 0) };
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<>("FromUtf16: Error in first call to WideCharToMultiByte"); }

			int cbMultiByte { rc + 4 };
			Enc::Write write = enc.IncWrite(NumCast<sizet>(cbMultiByte));
			rc = WideCharToMultiByte(outCodePage, 0, pStr, nWideChars, write.CharPtr(), cbMultiByte, 0, 0);
			if (rc <= 0)
				{ LastWinErr e; throw e.Make<>("FromUtf16: Error in second call to WideCharToMultiByte"); }

			// nWideChars may be -1 to indicate pStr is null-terminated.
			// In this case, WideCharToMultiByte will include and count the null terminator in its output.
			// We remove the null terminator here.
			if (nWideChars < 0 && rc > 0)
				--rc;

			write.AddSigned(rc);
		}
	}


	void StrCvtCp(Seq in, Str& out, UINT inCodePage, UINT outCodePage, Vec<wchar_t>& convertBuf)
	{
		out.Clear();
	
		if (in.n)
		{
			int inSize = NumCast<int>(in.n);
			int wcCount = MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, inSize, 0, 0);
			if (wcCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("StrCvtCp: Error in first call to MultiByteToWideChar"); }

			convertBuf.ResizeExact((sizet) wcCount);

			wcCount = MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, inSize, (LPWSTR) convertBuf.Ptr(), wcCount);
			if (wcCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("StrCvtCp: Error in second call to MultiByteToWideChar"); }

			convertBuf.ResizeExact((sizet) wcCount);
		
			int mbCount = WideCharToMultiByte(outCodePage, 0, (LPCWSTR) convertBuf.Ptr(), wcCount, 0, 0, 0, 0);
			if (mbCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("StrCvtCp: Error in first call to WideCharToMultiByte"); }

			out.ResizeExact((sizet) mbCount);

			mbCount = WideCharToMultiByte(outCodePage, 0, (LPCWSTR) convertBuf.Ptr(), wcCount, (LPSTR) out.Ptr(), mbCount, 0, 0);			
			if (mbCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("StrCvtCp: Error in second call to WideCharToMultiByte"); }

			out.ResizeExact((sizet) mbCount);
		}
	}


	void ToUtf8Norm(Seq in, Str& out, UINT inCodePage, Vec<wchar_t>& convertBuf1, Vec<wchar_t>& convertBuf2)
	{
		out.Clear();
	
		if (in.n)
		{
			// Convert to UTF-16
			int inSize = NumCast<int>(in.n);
			int wcCount = MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, inSize, 0, 0);
			if (wcCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("ToUtf8Norm: Error in first call to MultiByteToWideChar"); }

			convertBuf1.ResizeExact((sizet) wcCount);

			wcCount = MultiByteToWideChar(inCodePage, 0, (LPCSTR) in.p, inSize, convertBuf1.Ptr(), wcCount);
			if (wcCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("ToUtf8Norm: Error in second call to MultiByteToWideChar"); }

			convertBuf1.ResizeExact((sizet) wcCount);
		
			// Normalize UTF-16
			NormalizeUtf16(convertBuf1.Ptr(), wcCount, convertBuf2);
			wcCount = NumCast<int>(convertBuf2.Len());

			// Convert to UTF-8
			int mbCount = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) convertBuf2.Ptr(), wcCount, 0, 0, 0, 0);
			if (mbCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("ToUtf8Norm: Error in first call to WideCharToMultiByte"); }
		
			out.ResizeExact((sizet) mbCount);

			mbCount = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) convertBuf2.Ptr(), wcCount, (LPSTR) out.Ptr(), mbCount, 0, 0);			
			if (mbCount <= 0)
				{ LastWinErr e; throw e.Make<InputErr>("ToUtf8Norm: Error in second call to WideCharToMultiByte"); }
			
			out.ResizeExact((sizet) mbCount);
		}
	}

}
