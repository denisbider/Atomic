#include "AtIncludes.h"
#include "AtWinErr.h"

#include "AtUtfWin.h"


namespace At
{

	namespace
	{
		void TryAddWinErrDesc(Enc& enc, int64 err)
		{
			if (err >= INT32_MIN && err <= UINT32_MAX)
			{
				DWORD err32;
				if (err < 0)
					err32 = (DWORD) (((uint64) err) & 0xFFFFFFFFU);
				else
					err32 = (DWORD) err;

				wchar_t* msg {};
				DWORD    flags  { FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS };
				DWORD    langId { MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT) };
				DWORD    msgLen { FormatMessageW(flags, nullptr, err32, langId, (wchar_t*) &msg, 0, nullptr) };

				if (msgLen)
				{
					OnExit freeMsgBuf = [&] { LocalFree(msg); };

					enc.Add(": ");
					FromUtf16(msg, NumCast<int>(msgLen), enc, CP_UTF8);
				}
			}
		}
	} // anon


	void DescribeWinErr(Enc& enc, int64 err)
	{
		enc.Add("Windows error ").ErrCode(err);
		TryAddWinErrDesc(enc, err);
	}


	void DescribeNtStatus(Enc& enc, int64 err)
	{
		enc.Add("NT status ").ErrCode(err);
		if (err >= INT32_MIN && err <= UINT32_MAX)
		{
			int64 winErr = RtlNtStatusToDosError((NTSTATUS) err);
			if (winErr == ERROR_MR_MID_NOT_FOUND)
			{
				enc.Add(" (reinterpreted as Windows error)");
				TryAddWinErrDesc(enc, err);
			}
			else
			{
				enc.Add(" (mapped to Windows error ").ErrCode(winErr).Add(")");
				TryAddWinErrDesc(enc, winErr);
			}
		}
	}


	namespace
	{
		struct ErrCodeDesc
		{
			int m_err;
			char const* m_desc;
		};

		ErrCodeDesc const gc_preWin32FileOpErrs[] =
			{
				{    0x71, "DE_SAMEFILE: The source and destination files are the same file." },
				{    0x72, "DE_MANYSRC1DEST: Multiple file paths were specified in the source buffer, but only one destination file path." },
				{    0x73, "DE_DIFFDIR: Rename operation was specified but the destination path is a different directory. Use the move operation instead." },
				{    0x74, "DE_ROOTDIR: The source is a root directory, which cannot be moved or renamed." },
				{    0x75, "DE_OPCANCELLED: The operation was canceled by the user, or silently canceled if the appropriate flags were supplied to SHFileOperation." },
				{    0x76, "DE_DESTSUBTREE: The destination is a subtree of the source." },
				{    0x78, "DE_ACCESSDENIEDSRC: Security settings denied access to the source." },
				{    0x79, "DE_PATHTOODEEP: The source or destination path exceeded or would exceed MAX_PATH." },
				{    0x7A, "DE_MANYDEST: The operation involved multiple destination paths, which can fail in the case of a move operation." },
				{    0x7C, "DE_INVALIDFILES: The path in the source or destination or both was invalid." },
				{    0x7D, "DE_DESTSAMETREE: The source and destination have the same parent folder." },
				{    0x7E, "DE_FLDDESTISFILE: The destination path is an existing file." },
				{    0x80, "DE_FILEDESTISFLD: The destination path is an existing folder." },
				{    0x81, "DE_FILENAMETOOLONG: The name of the file exceeds MAX_PATH." },
				{    0x82, "DE_DEST_IS_CDROM: The destination is a read-only CD-ROM, possibly unformatted." },
				{    0x83, "DE_DEST_IS_DVD: The destination is a read-only DVD, possibly unformatted." },
				{    0x84, "DE_DEST_IS_CDRECORD: The destination is a writable CD-ROM, possibly unformatted." },
				{    0x85, "DE_FILE_TOO_LARGE: The file involved in the operation is too large for the destination media or file system." },
				{    0x86, "DE_SRC_IS_CDROM: The source is a read-only CD-ROM, possibly unformatted." },
				{    0x87, "DE_SRC_IS_DVD: The source is a read-only DVD, possibly unformatted." },
				{    0x88, "DE_SRC_IS_CDRECORD: The source is a writable CD-ROM, possibly unformatted." },
				{    0xB7, "DE_ERROR_MAX: MAX_PATH was exceeded during the operation." },
				{   0x402, "An unknown error occurred. This is typically due to an invalid path in the source or destination. Not expected to occur on Windows Vista and later." },
				{ 0x10000, "An unspecified error occurred on the destination." },
				{ 0x10074, "Destination is a root directory and cannot be renamed." },
				{}
			};
	} // anon

	char const* GetPreWin32FileOpErrDesc(int64 err)
	{
		for (ErrCodeDesc const* ecd = gc_preWin32FileOpErrs; ecd->m_err; ++ecd)
			if (ecd->m_err == err)
				return ecd->m_desc;

		return nullptr;
	}

}
