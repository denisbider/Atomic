#pragma once

#include "AtStr.h"
#include "AtTime.h"
#include "AtWinStr.h"


namespace At
{

	Str GetModulePath(HMODULE module = 0);



	struct PathParts
	{
		PathParts() {}
		PathParts(Seq path) { Parse(path); }
		PathParts& Parse(Seq path);

		Seq m_dir;
		Seq m_fileName;
		Seq m_baseName;
		Seq m_ext;
		Seq m_dirAndBaseName;
	};



	Seq GetDirectoryOfFileName(Seq path);

	Str JoinPath(Seq firstPath, Seq secondPath, char delim = '\\');

	Str GetModuleSubdir(Seq subdir, HMODULE module = 0);

	struct DirSecurity { enum E { SystemDefault, Restricted_AddFilesOnly, Restricted_FullAccess }; };
	void CreateDirectoryIfNotExists(Seq path, DirSecurity::E dirSecurity);

	void RemoveDirAndSubdirsIfExists(Seq path);

	// Returns true if the base name is of the form CON, PRN, AUX, NUL, COMx or LPTx
	bool IsRestrictedWindowsBaseName(Seq baseName);



	// A filename is benign if it is less than a certain length, and adheres to the following grammar:
	//
	// BenignFileName:    (BfnPartRestricted "/")* BfnPartRestricted [ "." BfnPart ]
	// BfnPartRestricted: BfnPart, except restricted Windows base names
	// BfnPart:           AlphaNumChars [ BfnPartSeparator AlphaNumChars ]
	// BfnPartSeparator:  "-" or "_"
	// AlphaNumChars:     AlphaNumChar [ AlphaNumChars ]
	// AlphaNumChar:      any of the ASCII characters a-z, A-Z, and 0-9
	//
	// Additional restrictions: BfnPart

	class BenignFileName
	{
	public:
		enum { MaxBenignFileNameLen = 199 };

		bool Parse(Seq fname, sizet maxNrSubDirs);
		Str JoinToPath(Seq path);

		Vec<Seq> m_subDirs;
		Seq      m_baseName;
		Seq      m_ext;

	private:
		static Seq ParsePart(Seq& fname);
	};



	class FindFiles
	{
	public:
		enum class InfoLevel { Standard  = FindExInfoStandard,    Basic    = FindExInfoBasic                };
		enum class SearchOp  { NameMatch = FindExSearchNameMatch, DirsOnly = FindExSearchLimitToDirectories };

		struct Flags { enum E : uint32 { CaseSensitive = FIND_FIRST_EX_CASE_SENSITIVE, LargeFetch = FIND_FIRST_EX_LARGE_FETCH }; };

		FindFiles(Seq pattern) : m_pattern(pattern) {}
		~FindFiles();

		FindFiles& SetInfoLevel (InfoLevel v) { EnsureThrow(m_h == INVALID_HANDLE_VALUE); m_infoLevel = (FINDEX_INFO_LEVELS) v; return *this; }
		FindFiles& SetSearchOp  (SearchOp  v) { EnsureThrow(m_h == INVALID_HANDLE_VALUE); m_searchOp  = (FINDEX_SEARCH_OPS)  v; return *this; }
		FindFiles& SetFlags     (DWORD     v) { EnsureThrow(m_h == INVALID_HANDLE_VALUE); m_flags     =                      v; return *this; }

		struct Result
		{
			DWORD  m_attrs             {};
			Time   m_creationTime;
			Time   m_lastAccessTime;
			Time   m_lastWriteTime;
			uint64 m_size              {};
			DWORD  m_reparsePointTag   {};
			Str    m_fileName;
			Str    m_alternateFileName;

			bool IsDirectory() const { return (m_attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY; }
		};

		bool Next();
		Result const& Current() const { EnsureThrow(m_h != INVALID_HANDLE_VALUE); return m_result; }

	private:
		WinStr             m_pattern;
		FINDEX_INFO_LEVELS m_infoLevel { FindExInfoBasic };
		FINDEX_SEARCH_OPS  m_searchOp  { FindExSearchNameMatch };
		DWORD              m_flags     {};
		HANDLE             m_h         { INVALID_HANDLE_VALUE };
		Result             m_result;
	};

}
