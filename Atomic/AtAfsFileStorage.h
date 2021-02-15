#pragma once

#include "AtAfs.h"
#include "AtCache.h"
#include "AtMap.h"
#include "AtStorageFile.h"


namespace At
{

	class AfsFileStorage : public AfsStorage
	{
	public:
		enum { MinBlockSize = 4096 };

		enum class Consistency { Unknown, NoFlush, Flush, Journal, VerifyJournal };

		// Returns the journal file path corresponding to the provided data file path
		static Str GetJournalFilePath(Seq dataFilePath);

		// If storage already exists, blockSize is ignored and may be invalid
		// Otherwise, to initialize new storage, blockSize must be a multiple of MinBlockSize
		void Init(Seq dataFileFullPath, uint32 blockSize, Consistency consistency);

		// Call after successful Init() to set a maximum size for the storage file
		// The maximum size excludes the journal file which may briefly grow to arbitrary size depending on use
		// Pass UINT64_MAX for unlimited size. If not called, default maximum size is unlimited
		void SetMaxSizeBytes(uint64 maxSizeBytes);

		uint64 NrCacheHits   () const { return m_nrCacheHits; }
		uint64 NrCacheMisses () const { return m_nrCacheMisses; }

	public:
		BlockAllocator& Allocator   () override final { return m_allocator; }
		uint64          MaxNrBlocks () override final { return m_maxNrBlocks; }
		uint64          NrBlocks    () override final { return m_nrBlocksStored + m_nrBlocksToAdd; }

		AfsResult::E AddNewBlock(AfsBlock& block) override final;
		AfsResult::E ObtainBlock(AfsBlock& block, uint64 blockIndex) override final;

		void BeginJournaledWrite() override final;
		void AbortJournaledWrite() noexcept override final;
		void CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite) override final;
		
	private:
		enum class State { Initial, Ready, JournaledWrite, Inconsistent };

		State          m_state              {};
		uint32         m_blockSize          {};
		uint64         m_maxNrBlocks        { UINT64_MAX };
		Consistency    m_consistency        {};
		BlockAllocator m_allocator;
		StorageFile    m_dataFile;
		JournalFile    m_journalFile;
		uint64         m_nrBlocksStored     {};
		sizet          m_nrBlocksToAdd      {};
		uint64         m_nrCacheHits        {};
		uint64         m_nrCacheMisses      {};

		sizet                      m_cacheTargetSize { 100 };
		Time                       m_cacheMaxAge     { Time::FromSeconds(60) };
		Cache<uint64, Rp<RcBlock>> m_cachedBlocks;
		OrderedSet<uint64>         m_blocksInUse;


		struct JournalOp { enum E : byte { Invalid = 0, Entry = 1, End = 2 }; };

		struct JournalEntry
		{
			uint64 m_blockIndex {};
			Seq    m_block;

			uint64 Key() const { return m_blockIndex; }
		};

		
		// Returns true if a valid journal record was read, regardless of number of entries
		bool ReadJournal(BlockMemory& blocks, Map<JournalEntry>& entries);

		void WriteJournal(Map<JournalEntry> const& entries);
		void ExecuteJournal(Map<JournalEntry>& entries);
		void ExecuteConsecutiveJournalEntries(Map<JournalEntry>::ConstIt it, Map<JournalEntry>::ConstIt const& itEnd, sizet nrEntries);
	};

}
