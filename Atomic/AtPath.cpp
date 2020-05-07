#include "AtIncludes.h"
#include "AtPath.h"

#include "AtException.h"
#include "AtWinErr.h"
#include "AtWinSecurity.h"
#include "AtWinStr.h"


namespace At
{

	Str GetModulePath(HMODULE module)
	{
		Vec<wchar_t> path(_MAX_PATH + 1);
		while (true)
		{
			DWORD result = GetModuleFileNameW(module, path.Ptr(), (DWORD) path.Len());
			if (!result)
				throw ZLitErr("GetModuleFileName() failed");

			if (result == (DWORD) path.Len())
				throw ZLitErr("GetModuleFileName() return value is too long");

			Str pathN;
			FromUtf16(path.Ptr(), NumCast<USHORT>(result), pathN, CP_UTF8);
			return std::move(pathN);
		}
	}


	void SplitStringAtLastSeparator(Seq s, uint separator, Seq& head, Seq& tail, Seq& dominantPart)
	{
		head.Set(s.p, 0);
		tail.Set(s.p + s.n, 0);
		dominantPart = s;

		Seq reader { s };
		while (true)
		{
			reader.DropToByte(separator);
			if (!reader.n)
				break;
		
			head.Set(s.p, (sizet) (reader.p - s.p));
			tail = reader.DropByte();
		}
	}

	PathParts& PathParts::Parse(Seq path)
	{
		SplitStringAtLastSeparator(path, '\\', m_dir, m_fileName, m_fileName);
		SplitStringAtLastSeparator(m_fileName, '.', m_baseName, m_ext, m_baseName);
		m_dirAndBaseName.Set(path.p, (m_baseName.p - path.p) + m_baseName.n);
		return *this;
	}

	Seq GetDirectoryOfFileName(Seq path)
	{
		Seq dir, fileName;
		SplitStringAtLastSeparator(path, '\\', dir, fileName, fileName);
		return dir;
	}


	Str JoinPath(Seq firstPath, Seq secondPath, char delim)
	{
		if (!firstPath.n || !secondPath.n)
			return Str(firstPath).Add(secondPath);
		if (firstPath.p[firstPath.n - 1] == delim && secondPath.p[0] == delim)
			return Str(firstPath).Add(Seq(secondPath).DropByte());
		if (firstPath.p[firstPath.n - 1] == delim || secondPath.p[0] == delim)
			return Str(firstPath).Add(secondPath);
		return Str(firstPath).Ch(delim).Add(secondPath);
	}


	Str GetModuleSubdir(Seq subdir, HMODULE module)
	{
		Str modulePath(GetModulePath(module));
		return JoinPath(GetDirectoryOfFileName(modulePath), subdir);
	}


	void CreateDirectoryIfNotExists(Seq path, DirSecurity::E dirSecurity)
	{
		WinStr pathW { path };
		DWORD  attr  { GetFileAttributesW(pathW.Z()) };
		if (attr != INVALID_FILE_ATTRIBUTES)
		{
			if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
				throw StrErr(Str("CreateDirectoryIfNotExists for '").Add(path).Add("': the path already exists, but is not a directory"));
		}
		else
		{
			SecDescriptor sd;
			SecAttrs sa;
			LPSECURITY_ATTRIBUTES saPtr {};

			if (dirSecurity != DirSecurity::SystemDefault)
			{
				TokenInfoOfProcess<TokenInfo_Owner> owner { GetCurrentProcess() };
				StringSid ownerStringSid { owner.Ptr()->Owner };
				Seq sid { ownerStringSid.GetStr() };

				Str sddl;
				sddl.ReserveExact(1000);
				sddl.Set("D:PAI"										// DACL, SE_DACL_PROTECTED, SE_DACL_AUTO_INHERITED
						 "(A;OICI;FA;;;BA)"								// Allow, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, FILE_ALL_ACCESS, Built-in Administrators
						 "(A;OICI;FA;;;SY)");							// Allow, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, FILE_ALL_ACCESS, Local System

				if (dirSecurity == DirSecurity::Restricted_AddFilesOnly)
					sddl.Add("(A;CI;0x100002;;;").Add(sid).Add(")");	// Allow, CONTAINER_INHERIT_ACE, SYNCHRONIZE | FILE_WRITE_ACCESS, our SID - allow us to create new files, but not read/modify existing ones
				else if (dirSecurity == DirSecurity::Restricted_FullAccess)
					sddl.Add("(A;OICI;FA;;;").Add(sid).Add(")");		// Allow, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, FILE_ALL_ACCESS, our SID
				else
					EnsureThrow(!"Unrecognized directory security type");

				sd.Set(sddl);
				sa.Set(sd, InheritHandle::No);
				saPtr = sa.Ptr();
			}

			if (!CreateDirectoryW(pathW.Z(), saPtr))
			{
				DWORD rc = GetLastError();
				throw WinErr<>(rc, Str("CreateDirectoryIfNotExists for '").Add(path).Add("': CreateDirectory failed"));
			}
		}
	}


	void RemoveDirAndSubdirsIfExists(Seq path)
	{
		WinStr pathW { path };

		Vec<wchar_t> pathW2 { pathW.Len() + 2 };	// Double null-terminated for SHFileOperation
		Mem::Copy<wchar_t>(pathW2.Ptr(), pathW.Z(), pathW.Len());

		DWORD storePathAttrs = GetFileAttributesW(pathW.Z());
		if (storePathAttrs != INVALID_FILE_ATTRIBUTES)
		{
			if (!(storePathAttrs & FILE_ATTRIBUTE_DIRECTORY))
				throw StrErr(Str("RemoveDirAndSubdirsIfExists: Specified path exists, but is not a directory: ").Add(path));
	
			SHFILEOPSTRUCT fileOp {};
			fileOp.wFunc  = FO_DELETE;
			fileOp.pFrom  = pathW2.Ptr();
			fileOp.fFlags = FOF_NO_UI;

			int rc = SHFileOperationW(&fileOp);
			if (rc != 0)
				throw WinFileOpErr<>(rc, "SHFileOperation");
		}
	}


	bool IsRestrictedWindowsBaseName(Seq baseName)
	{
		if (baseName.EqualInsensitive("CON")) return true;
		if (baseName.EqualInsensitive("PRN")) return true;
		if (baseName.EqualInsensitive("AUX")) return true;
		if (baseName.EqualInsensitive("NUL")) return true;
		if (baseName.StripPrefixInsensitive("COM") && baseName.StartsWithAnyOfType(Ascii::IsDecDigit)) return true;
		if (baseName.StripPrefixInsensitive("LPT") && baseName.StartsWithAnyOfType(Ascii::IsDecDigit)) return true;
		return false;
	}



	// BenignFileName

	bool BenignFileName::Parse(Seq fname, sizet maxNrSubDirs)
	{
		m_subDirs.Clear();
		m_baseName = Seq();
		m_ext      = Seq();

		if (fname.n < 1)                    return false;
		if (fname.n > MaxBenignFileNameLen) return false;

		if (maxNrSubDirs)
		{
			Seq subDirs = fname.ReadToAfterLastByte('/');
			if (subDirs.n)
			{
				sizet subDirsCap = PickMin(maxNrSubDirs, (subDirs.n + 1) / 2);
				m_subDirs.ReserveExact(subDirsCap);

				sizet nrSubDirs {};
				while (true)
				{
					Seq subDir = ParsePart(subDirs);
					if (!subDir.n)                           return false;
					if (IsRestrictedWindowsBaseName(subDir)) return false;
					if (++nrSubDirs > maxNrSubDirs)          return false;

					m_subDirs.Add(subDir);
					if (subDirs.ReadByte() != '/')           return false;
					if (!subDirs.n)                          break;
				}
			}
		}

		m_baseName = ParsePart(fname);
		if (!m_baseName.n)                           return false;
		if (IsRestrictedWindowsBaseName(m_baseName)) return false;

		if (fname.n)
		{
			if (fname.ReadByte() != '.') return false;

			m_ext = ParsePart(fname);
			if (!m_ext.n)                return false;
			if (fname.n != 0)            return false;
		}

		return true;
	}


	Seq BenignFileName::ParsePart(Seq& fname)
	{
		Seq part { fname.p, 0 };
		while (true)
		{
			Seq chunk = fname.ReadToFirstByteNotOfType(Ascii::IsAlphaNum);
			if (!chunk.n)
				return Seq();

			part.n += chunk.n;
			if (!fname.n)
				return part;
		
			if (fname.p[0] != '-' && fname.p[0] != '_')
				return part;

			fname.DropByte();
			++(part.n);
		}
	}


	Str BenignFileName::JoinToPath(Seq path)
	{
		sizet cap = path.n + 1 + m_baseName.n + 1 + m_ext.n;
		for (Seq subDir : m_subDirs)
			cap += subDir.n + 1;

		Str result;
		result.ReserveExact(cap).Add(path);
		if (!path.EndsWithExact("\\"))
			result.Ch('\\');

		for (Seq subDir : m_subDirs)
			result.Add(subDir).Ch('\\');

		result.Add(m_baseName);
		if (m_ext.n)
			result.Ch('.').Add(m_ext);

		return result;
	}



	// FindFiles

	FindFiles::~FindFiles()
	{
		if (m_h != INVALID_HANDLE_VALUE)
		{
			if (!FindClose(m_h))
			{
				DWORD err = GetLastError();
				EnsureReportWithNr(!"Error in FindClose", err);
			}
		}
	}

	
	bool FindFiles::Next()
	{
		WIN32_FIND_DATAW d;
		if (m_h == INVALID_HANDLE_VALUE)
		{
			m_h = FindFirstFileExW(m_pattern.Z(), m_infoLevel, &d, m_searchOp, nullptr, m_flags);
			if (m_h == INVALID_HANDLE_VALUE)
			{
				LastWinErr e;
				if (e.m_err == ERROR_FILE_NOT_FOUND || e.m_err == ERROR_PATH_NOT_FOUND)
					return false;

				throw e.Make<>(__FUNCTION__ ": Error in FindFirstFileEx");
			}
		}
		else
		{
			if (!FindNextFileW(m_h, &d))
			{
				LastWinErr e;
				if (e.m_err == ERROR_NO_MORE_FILES)
					return false;

				throw e.Make<>(__FUNCTION__ ": Error in FindNextFile");
			}
		}

		m_result.m_attrs           = d.dwFileAttributes;
		m_result.m_creationTime    = Time::FromFt(d.ftCreationTime   );
		m_result.m_lastAccessTime  = Time::FromFt(d.ftLastAccessTime );
		m_result.m_lastWriteTime   = Time::FromFt(d.ftLastWriteTime  );
		m_result.m_size            = (((uint64) d.nFileSizeHigh) << 32) | ((uint64) d.nFileSizeLow);
		m_result.m_reparsePointTag = d.dwReserved0;
		FromUtf16(d.cFileName,          -1, m_result.m_fileName.Clear(),          CP_UTF8);
		FromUtf16(d.cAlternateFileName, -1, m_result.m_alternateFileName.Clear(), CP_UTF8);
		return true;
	}

}
