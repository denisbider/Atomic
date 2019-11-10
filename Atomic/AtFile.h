#pragma once

#include "AtStr.h"
#include "AtTime.h"


namespace At
{

	class File : NoCopy
	{
	public:
		struct Share {
			static DWORD const Read              = FILE_SHARE_READ;
			static DWORD const Write             = FILE_SHARE_WRITE;
			static DWORD const Delete            = FILE_SHARE_DELETE;
			};
		
		struct Disp { 
			static DWORD const CreateNew         = CREATE_NEW;
			static DWORD const CreateAlways      = CREATE_ALWAYS;
			static DWORD const OpenExisting      = OPEN_EXISTING;
			static DWORD const OpenAlways        = OPEN_ALWAYS;
			static DWORD const TruncateExisting  = TRUNCATE_EXISTING;
			};

		struct Attr {
			static DWORD const ReadOnly          = FILE_ATTRIBUTE_READONLY;
			static DWORD const Hidden            = FILE_ATTRIBUTE_HIDDEN;
			static DWORD const System            = FILE_ATTRIBUTE_SYSTEM;
			static DWORD const Directory         = FILE_ATTRIBUTE_DIRECTORY;
			static DWORD const Archive           = FILE_ATTRIBUTE_ARCHIVE;
			static DWORD const Device            = FILE_ATTRIBUTE_DEVICE;
			static DWORD const Normal            = FILE_ATTRIBUTE_NORMAL;
			static DWORD const Temporary         = FILE_ATTRIBUTE_TEMPORARY;
			static DWORD const SparseFile        = FILE_ATTRIBUTE_SPARSE_FILE;
			static DWORD const ReparsePoint      = FILE_ATTRIBUTE_REPARSE_POINT;
			static DWORD const Compressed        = FILE_ATTRIBUTE_COMPRESSED;
			static DWORD const Offline           = FILE_ATTRIBUTE_OFFLINE;
			static DWORD const NotContentIndexed = FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
			static DWORD const Encrypted         = FILE_ATTRIBUTE_ENCRYPTED;
			static DWORD const IntegrityStream   = FILE_ATTRIBUTE_INTEGRITY_STREAM;
			static DWORD const Virtual           = FILE_ATTRIBUTE_VIRTUAL;
			static DWORD const NoScrubData       = FILE_ATTRIBUTE_NO_SCRUB_DATA;
			static DWORD const EA                = FILE_ATTRIBUTE_EA;
			};

		struct Flag {
			static DWORD const FirstPipeInstance = FILE_FLAG_FIRST_PIPE_INSTANCE;
			static DWORD const OpenNoRecall      = FILE_FLAG_OPEN_NO_RECALL;
			static DWORD const OpenReparsePoint  = FILE_FLAG_OPEN_REPARSE_POINT;
			static DWORD const SessionAware      = FILE_FLAG_SESSION_AWARE;
			static DWORD const PosixSemantics    = FILE_FLAG_POSIX_SEMANTICS;
			static DWORD const BackupSemantics   = FILE_FLAG_BACKUP_SEMANTICS;
			static DWORD const DeleteOnClose     = FILE_FLAG_DELETE_ON_CLOSE;
			static DWORD const SequentialScan    = FILE_FLAG_SEQUENTIAL_SCAN;
			static DWORD const RandomAccess      = FILE_FLAG_RANDOM_ACCESS;
			static DWORD const NoBuffering       = FILE_FLAG_NO_BUFFERING;
			static DWORD const Overlapped        = FILE_FLAG_OVERLAPPED;
			static DWORD const WriteThrough      = FILE_FLAG_WRITE_THROUGH;
			};

		struct OpenArgs
		{
			DWORD                 m_access       {};
			DWORD                 m_share        {};
			LPSECURITY_ATTRIBUTES m_secAttrs     {};
			DWORD                 m_disp         {};
			DWORD                 m_flagsAttrs   {};
			HANDLE                m_templateFile {};

			OpenArgs& Access       (DWORD                 n) { m_access       = n; return *this; }
			OpenArgs& Share        (DWORD                 n) { m_share        = n; return *this; }
			OpenArgs& SecAttrs     (LPSECURITY_ATTRIBUTES s) { m_secAttrs     = s; return *this; }
			OpenArgs& Disp         (DWORD                 n) { m_disp         = n; return *this; }
			OpenArgs& FlagsAttrs   (DWORD                 n) { m_flagsAttrs   = n; return *this; }
			OpenArgs& TemplateFile (HANDLE                h) { m_templateFile = h; return *this; }

			static OpenArgs DefaultRead      () { return OpenArgs().Access(GENERIC_READ ).Share(Share::Read | Share::Delete).Disp(Disp::OpenExisting); }
			static OpenArgs DefaultOverwrite () { return OpenArgs().Access(GENERIC_WRITE).Share(Share::Read | Share::Delete).Disp(Disp::CreateAlways); }
		};

		struct Times
		{
			Time m_creationTime;
			Time m_lastAccessTime;
			Time m_lastWriteTime;
		};

		struct AttrsEx
		{
			DWORD  m_err {};
			DWORD  m_attrs {};
			Times  m_times;
			uint64 m_size {};
		};

		static DWORD GetAttrs(Seq path);
		static bool GetAttrsEx(Seq path, AttrsEx& x);
		static bool Exists_NotDirectory(Seq path);
		static bool Exists_IsDirectory(Seq path);
		static void Move(Seq oldPath, Seq newPath);

		File() = default;
		File(File&& x) noexcept : m_pathOpened(std::move(x.m_pathOpened)), m_hFile(x.m_hFile) { x.m_hFile = INVALID_HANDLE_VALUE; }
		~File() noexcept;

		File&  Open             (Seq path, OpenArgs const& args);
		File&  Open             (wchar_t const* wzPath, OpenArgs const& args);
		bool   IsOpen           () const { return m_hFile != INVALID_HANDLE_VALUE; }
		Seq    PathOpened       () const { return m_pathOpened; }
		uint64 GetSize          () const;
		Times  GetTimes         () const;
		HANDLE Handle           () { return m_hFile; }
		File&  ReadInto         (Enc& enc, DWORD nrBytes);
		File&  ReadAllInto      (Enc& enc) { return ReadInto(enc, NumCast<DWORD>(GetSize())); }
		File&  Write            (void const* pv, sizet n);
		File&  Write            (Seq s) { return Write(s.p, s.n); }

	protected:
		Str    m_pathOpened;
		HANDLE m_hFile { INVALID_HANDLE_VALUE };

		void OpenInner(wchar_t const* wzPath, OpenArgs const& args);
		Str DescribeFileError(Seq method, Seq api) const;
	};


	
	class FileLoader
	{
	public:
		FileLoader(Seq path) : m_path(path)
			{ File().Open(path, File::OpenArgs::DefaultRead()).ReadAllInto(m_content); }

		Seq Path    () const { return m_path;    }
		Seq Content () const { return m_content; }

	private:
		Str m_path;
		Str m_content;
	};

}
