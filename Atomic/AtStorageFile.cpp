#include "AtIncludes.h"
#include "AtStorageFile.h"

#include "AtWinErr.h"


namespace At
{

	// StorageFile

	void StorageFile::Open(WriteThrough writeThrough, Uncached uncached)
	{
		EnsureThrow(m_fullPath.Any());
		EnsureThrow(0 != m_blockSize);

		if (m_oldFullPaths.Any())
			CheckOldPathsAndRename();

		DWORD flags = 0;
		if (WriteThrough::Yes == writeThrough)
			flags |= File::Flag::WriteThrough;
		if (Uncached::Yes == uncached)
			flags |= File::Flag::NoBuffering;

		File::Open(m_fullPath, OpenArgs().Access(GENERIC_READ | GENERIC_WRITE)
										 .Share(File::Share::Read)
										 .Disp(File::Disp::OpenAlways)
										 .FlagsAttrs(flags));
		
		m_writeThrough = writeThrough;
		m_uncached = uncached;
		m_fileSize = GetSize();
	}


	void StorageFile::CheckOldPathsAndRename()
	{
		bool anyOldPathExists {};
		Seq oldPathThatExists;
		for (Str const& oldPath : m_oldFullPaths)
			if (File::Exists_NotDirectory(oldPath))
				if (anyOldPathExists)
					EnsureFailWithDesc(OnFail::Abort, "A storage file exists under more than one old file name", __FUNCTION__, __LINE__);
				else
				{
					anyOldPathExists = true;
					oldPathThatExists = oldPath;
				}

		if (anyOldPathExists)
		{
			if (File::Exists_NotDirectory(m_fullPath))
				EnsureFailWithDesc(OnFail::Abort, "A storage file exists under both an old and a new file name", __FUNCTION__, __LINE__);

			File::Move(oldPathThatExists, m_fullPath);
		}
	}


	void StorageFile::ReadBlocks(void* firstBlock, sizet nrBlocks, uint64 offset)
	{
		EnsureThrow((offset % MinSectorSize) == 0);
		EnsureThrow(nrBlocks < (MAXDWORD / m_blockSize));
		DWORD bytesToRead = (DWORD) (nrBlocks * m_blockSize);
		ReadInner(firstBlock, bytesToRead, offset);
	}


	void StorageFile::ReadBytesUnaligned(void* pDestination, sizet bytesToRead, uint64 offset)
	{
		EnsureThrow(Uncached::No == m_uncached);
		EnsureThrow(bytesToRead < MAXDWORD);
		ReadInner(pDestination, (DWORD) bytesToRead, offset);
	}


	void StorageFile::ReadInner(void* pDestination, DWORD bytesToRead, uint64 offset)
	{

		DWORD bytesRead = 0;
		if (offset < m_fileSize)
		{
			if (offset != m_lastOffset)
			{
				LARGE_INTEGER li;
				li.QuadPart = NumCast<LONGLONG>(offset);
				if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
					{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
			}

			if (!ReadFile(m_hFile, pDestination, bytesToRead, &bytesRead, 0))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "ReadFile")); }
		}

		if (bytesRead != bytesToRead)
		{
			EnsureAbort(bytesRead <= bytesToRead);
			byte* pZero = ((byte*) pDestination) + bytesRead;
			sizet nZero = bytesToRead - bytesRead;
			EnsureThrow(pZero + nZero == ((byte*) pDestination) + bytesToRead);
			Mem::Zero(pZero, nZero);
		}

		m_lastOffset = offset + bytesRead;
	}


	void StorageFile::WriteBlocks(void const* firstBlock, sizet nrBlocks, uint64 offset)
	{
		EnsureThrow(offset <= m_fileSize);
		EnsureThrow((offset % MinSectorSize) == 0);
		EnsureThrow(nrBlocks < (MAXDWORD / m_blockSize));

		if (offset != m_lastOffset)
		{
			LARGE_INTEGER li;
			li.QuadPart = NumCast<LONGLONG>(offset);
			if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
		}

		DWORD bytesToWrite = (DWORD) (nrBlocks * m_blockSize);
		DWORD bytesWritten = 0;
		if (!WriteFile(m_hFile, firstBlock, bytesToWrite, &bytesWritten, 0))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "WriteFile")); }
		EnsureAbort(bytesWritten == bytesToWrite);

		m_lastOffset = offset + bytesWritten;

		if (m_fileSize < m_lastOffset)
			m_fileSize = m_lastOffset;
	}


	void StorageFile::SetEof(uint64 offset)
	{
		EnsureThrow((offset % MinSectorSize) == 0);

		if (offset != m_lastOffset)
		{
			LARGE_INTEGER li;
			li.QuadPart = NumCast<LONGLONG>(offset);
			if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
		}

		if (!SetEndOfFile(m_hFile))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetEndOfFile")); }

		m_lastOffset = offset;
		m_fileSize = offset;
	}



	// JournalFile

	JournalFile::~JournalFile()
	{
		if (m_clearBlock)
			m_allocator->ReleaseBlock(m_clearBlock);
	}


	void JournalFile::Clear()
	{
		EnsureThrow(INVALID_HANDLE_VALUE != m_hFile);
		EnsureThrow(nullptr != m_allocator);
	
		if (!m_clearBlock)
			m_clearBlock = m_allocator->GetBlock();

		WriteBlocks(m_clearBlock, 1, 0);
		SetEof(m_blockSize);
	}

}
