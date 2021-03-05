#pragma once

#include "AtBlockAllocator.h"
#include "AtException.h"
#include "AtFile.h"


namespace At
{

	class StorageFile : public File
	{
	public:
		struct SimErrDecider
		{
			virtual bool DecideIoSimErr() = 0;
		};

		struct IoErr          : public StrErr { IoErr          (Seq msg) : StrErr (msg) {} };
		struct SimulatedIoErr : public IoErr  { SimulatedIoErr (Seq msg) : IoErr  (msg) {} };

		enum { MinSectorSize = 512 };
		enum class WriteThrough { Unknown, No, Yes };
		enum class Uncached { Unknown, No, Yes };

		void         SetId              (byte id)          { m_id = id; }
		byte         Id                 () const           { return m_id; }

		// MUST be called before opening the file to set an initial block size. MAY be called after to change block size
		void         SetBlockSize       (sizet n)          { m_blockSize = n; }
		sizet        BlockSize          () const           { return m_blockSize; }

		// Used to simulate IO errors for testing purposes. Pass nullptr to clear a previously set SimErrDecider
		void         SetSimErrDecider   (SimErrDecider* d) { m_simErrDecider = d; }
		uint64       NrSimulatedIoErrs  () const           { return m_nrSimulatedIoErrs; }

		void         AddOldFullPath     (Seq fullPath)     { m_oldFullPaths.Add(fullPath); }
		void         SetFullPath        (Seq fullPath)     { m_fullPath = fullPath; }
		Seq          FullPath           () const           { return m_fullPath; }

		void         Open(WriteThrough writeThrough, Uncached uncached);

		bool         IsWriteThrough     () const           { return WriteThrough::Yes == m_writeThrough; }
		bool         IsUncached         () const           { return Uncached::Yes == m_uncached; }
		uint64       FileSize           () const           { return m_fileSize; }

		void         ReadBlocks         (void* firstBlock, sizet nrBlocks, uint64 offset);
		void         ReadBytesUnaligned (void* pDestination, sizet bytesToRead, uint64 offset);
		void         WriteBlocks        (void const* firstBlock, sizet nrBlocks, uint64 offset);
		void         SetEof             (uint64 offset);

	protected:
		byte           m_id                { 255 };			// Must equal ObjectStore::FileId::None
		sizet          m_blockSize         {};
		Vec<Str>       m_oldFullPaths;
		Str            m_fullPath;
		WriteThrough   m_writeThrough      {};
		Uncached       m_uncached          {};
		uint64         m_fileSize          {};
		uint64         m_lastOffset        {};
		SimErrDecider* m_simErrDecider     {};
		uint64         m_nrSimulatedIoErrs {};

		void CheckOldPathsAndRename();
		void ReadInner(void* pDestination, DWORD bytesToRead, uint64 offset);
		void SimulateIoErr(char const* zOp, char const* zLoc, long line);
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
