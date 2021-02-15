#pragma once

#include "AtFile.h"
#include "AtBlockAllocator.h"


namespace At
{

	// StorageFile

	class StorageFile : public File
	{
	public:
		enum { MinSectorSize = 512 };
		enum class WriteThrough { Unknown, No, Yes };
		enum class Uncached { Unknown, No, Yes };

		void         SetId          (byte id)      { m_id = id; }
		byte         Id             () const       { return m_id; }

		// MUST be called before opening the file to set an initial block size. MAY be called after to change block size
		void         SetBlockSize   (sizet n)      { m_blockSize = n; }
		sizet        BlockSize      () const       { return m_blockSize; }

		void         AddOldFullPath (Seq fullPath) { m_oldFullPaths.Add(fullPath); }
		void         SetFullPath    (Seq fullPath) { m_fullPath = fullPath; }
		Seq          FullPath       () const       { return m_fullPath; }

		void         Open(WriteThrough writeThrough, Uncached uncached);

		bool         IsWriteThrough () const       { return WriteThrough::Yes == m_writeThrough; }
		bool         IsUncached     () const       { return Uncached::Yes == m_uncached; }
		uint64       FileSize       () const       { return m_fileSize; }

		void         ReadBlocks         (void* firstBlock, sizet nrBlocks, uint64 offset);
		void         ReadBytesUnaligned (void* pDestination, sizet bytesToRead, uint64 offset);
		void         WriteBlocks        (void const* firstBlock, sizet nrBlocks, uint64 offset);
		void         SetEof             (uint64 offset);

	protected:
		byte         m_id			{ 255 };			// ObjectStore::FileId::None
		sizet        m_blockSize    {};
		Vec<Str>     m_oldFullPaths;
		Str          m_fullPath;
		WriteThrough m_writeThrough {};
		Uncached     m_uncached     {};
		uint64       m_fileSize	    {};
		uint64       m_lastOffset   {};

		void CheckOldPathsAndRename();
		void ReadInner(void* pDestination, DWORD bytesToRead, uint64 offset);
	};

	class StorageFile_OsCached : public StorageFile
		{ public: void Open() { StorageFile::Open(WriteThrough::Yes, Uncached::No); } };

	class StorageFile_Uncached : public StorageFile
		{ public: void Open() { StorageFile::Open(WriteThrough::Yes, Uncached::Yes); } };



	// JournalFile

	class JournalFile : public StorageFile_Uncached
	{
	public:
		~JournalFile();

		void SetAllocator(BlockAllocator& allocator) { EnsureThrow(m_blockSize <= allocator.BytesPerBlock()); m_allocator = &allocator; }
		void Clear();

	private:
		BlockAllocator* m_allocator {};
		void*           m_clearBlock {};
	};

}
