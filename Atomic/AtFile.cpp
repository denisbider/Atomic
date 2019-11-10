#include "AtIncludes.h"
#include "AtFile.h"

#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	DWORD File::GetAttrs(Seq path)
	{
		WinStr pathW { path };
		DWORD attrs = GetFileAttributesW(pathW.Z());
		return attrs;
	}


	bool File::GetAttrsEx(Seq path, AttrsEx& x)
	{
		WinStr pathW { path };
		WIN32_FILE_ATTRIBUTE_DATA a;
		if (!GetFileAttributesExW(pathW.Z(), GetFileExInfoStandard, &a))
		{
			x.m_err = GetLastError();
			return false;
		}

		x.m_err = 0;
		x.m_attrs = a.dwFileAttributes;
		x.m_times.m_creationTime   = Time::FromFt(a.ftCreationTime);
		x.m_times.m_lastAccessTime = Time::FromFt(a.ftLastAccessTime);
		x.m_times.m_lastWriteTime  = Time::FromFt(a.ftLastWriteTime);
		x.m_size = (((uint64) a.nFileSizeHigh) << 32) | ((uint64) a.nFileSizeLow);
		return true;
	}


	bool File::Exists_NotDirectory(Seq path)
	{
		DWORD attrs = GetAttrs(path);
		if (attrs != INVALID_FILE_ATTRIBUTES)
			if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
				return true;
		return false;
	}


	bool File::Exists_IsDirectory(Seq path)
	{
		DWORD attrs = GetAttrs(path);
		if (attrs != INVALID_FILE_ATTRIBUTES)
			if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
				return true;
		return false;
	}
	

	void File::Move(Seq oldPath, Seq newPath)
	{
		WinStr oldPathW { oldPath };
		WinStr newPathW { newPath };
		if (!MoveFileW(oldPathW.Z(), newPathW.Z()))
			{ LastWinErr e; throw e.Make<>(Str::Join(__FUNCTION__ ": Error in MoveFileW from \"", oldPath, "\" to \"", newPath, "\"")); }
	}


	File::~File() noexcept
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
			EnsureReportWithCode(CloseHandle(m_hFile), GetLastError());
	}


	File& File::Open(Seq path, OpenArgs const& args)
	{
		m_pathOpened.Set(path);
		OpenInner(WinStr(path).Z(), args);
		return *this;
	}


	File& File::Open(wchar_t const* wzPath, OpenArgs const& args)
	{
		m_pathOpened.Clear();
		FromUtf16(wzPath, NumCast<int>(ZLen(wzPath)), m_pathOpened, CP_UTF8);
		OpenInner(wzPath, args);
		return *this;
	}


	void File::OpenInner(wchar_t const* wzPath, OpenArgs const& args)
	{
		EnsureThrow(m_hFile == INVALID_HANDLE_VALUE);

		m_hFile = CreateFileW(wzPath, args.m_access, args.m_share, args.m_secAttrs, args.m_disp, args.m_flagsAttrs, args.m_templateFile);
		if (m_hFile == INVALID_HANDLE_VALUE)
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "CreateFileW")); }
	}


	uint64 File::GetSize() const
	{
		EnsureThrow(m_hFile != INVALID_HANDLE_VALUE);

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(m_hFile, &fileSize))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "GetFileSizeEx")); }

		return NumCast<uint64>(fileSize.QuadPart);
	}


	File::Times File::GetTimes() const
	{
		FILETIME ftCreate, ftLastAccess, ftLastWrite;
		if (!GetFileTime(m_hFile, &ftCreate, &ftLastAccess, &ftLastWrite))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "GetFileTime")); }

		Times t;
		t.m_creationTime   = Time::FromFt(ftCreate);
		t.m_lastAccessTime = Time::FromFt(ftLastAccess);
		t.m_lastWriteTime  = Time::FromFt(ftLastWrite);
		return t;
	}


	File& File::ReadInto(Enc& enc, DWORD nrBytes)
	{
		Enc::Write write = enc.IncWrite(nrBytes);
		DWORD nrBytesRead {};

		if (!ReadFile(m_hFile, write.Ptr(), nrBytes, &nrBytesRead, nullptr))
			{ LastWinErr e; throw e.Make<>(DescribeFileError("File::ReadAllInto", "ReadFile")); }

		write.Add(nrBytesRead);
		return *this;
	}


	File& File::Write(void const* pv, sizet n)
	{
		DWORD bytesWritten {};
		if (!WriteFile(m_hFile, pv, NumCast<DWORD>(n), &bytesWritten, nullptr))
			{ LastWinErr e; throw e.Make<>(DescribeFileError("File::Write", "WriteFile")); }

		if (bytesWritten != n)
			throw StrErr(DescribeFileError("File::Write", "WriteFile").Add(": Unexpected number of bytes written"));

		return *this;
	}


	Str File::DescribeFileError(Seq method, Seq api) const
	{
		Str s { method };
		s.Add(": Error in ").Add(api);
		if (m_pathOpened.Any())
			s.Add(" for \"").Add(m_pathOpened).Add("\"");
		return s;
	}

}
