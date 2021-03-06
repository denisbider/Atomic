#include "AtIncludes.h"
#include "AtAfsFileStorage.h"

#include "AtEncode.h"
#include "AtPath.h"
#include "AtWinErr.h"


namespace At
{

	Str AfsFileStorage::GetJournalFilePath(Seq dataFilePath)
	{
		PathParts pathParts;
		pathParts.Parse(dataFilePath);
		return Str::Join(pathParts.m_dirAndBaseName, ".jnl");
	}


	void AfsFileStorage::Init(Seq dataFileFullPath, uint32 createBlockSize, Consistency consistency)
	{
		EnsureThrow(State::Initial == m_state);
		EnsureThrow(dataFileFullPath.n);

		m_consistency = consistency;
		StorageFile::WriteThrough writeThrough = StorageFile::WriteThrough::Yes;
		if (consistency < Consistency::Journal)
			writeThrough = StorageFile::WriteThrough::No;

		m_allocator.SetBytesPerBlock(MinBlockSize);
		m_dataFile.SetBlockSize(MinBlockSize);
		m_dataFile.SetFullPath(dataFileFullPath);
		m_dataFile.Open(writeThrough, StorageFile::Uncached::No);

		if (m_consistency >= Consistency::Journal)
		{
			Str journalFileFullPath = GetJournalFilePath(dataFileFullPath);
			m_journalFile.SetBlockSize(MinBlockSize);
			m_journalFile.SetFullPath(journalFileFullPath);
			m_journalFile.Open();
		}

		Seq const signature = "AfsFileStorage\x1A";
		if (0 == m_dataFile.GetSize())
		{
			// Create new storage
			EnsureThrow(0 != createBlockSize);
			EnsureThrow(0 == (createBlockSize % MinBlockSize));

			m_blockSize = createBlockSize;

			AutoBlock block { m_allocator };
			byte* p = block.Ptr();
			Mem::Copy<byte>(p, signature.p, signature.n); p += signature.n;
			EncodeUInt32LE_Ptr(p, m_blockSize);

			m_dataFile.WriteBlocks(block.Ptr(), 1, 0);
			m_nrBlocksStored = 0;
		}
		else
		{
			// Initialize existing storage
			AutoBlock block { m_allocator };
			m_dataFile.ReadBlocks(block.Ptr(), 1, 0);

			Seq reader { block.Ptr(), MinBlockSize };
			if (!reader.StripPrefixExact(signature))                     throw StrErr(__FUNCTION__ ": Invalid signature");
			if (!DecodeUInt32LE(reader, m_blockSize))                    throw StrErr(__FUNCTION__ ": Could not decode block size");
			if (0 == m_blockSize || (0 != (m_blockSize % MinBlockSize))) throw StrErr(Str(__FUNCTION__ ": Invalid block size: ").UInt(m_blockSize));

			m_nrBlocksStored = SatSub<uint64>(m_dataFile.GetSize(), MinBlockSize) / m_blockSize;
		}

		m_allocator.SetBytesPerBlock(m_blockSize);
		m_dataFile.SetBlockSize(m_blockSize);

		if (m_consistency >= Consistency::Journal)
		{
			m_journalFile.SetBlockSize(m_blockSize);
			m_journalFile.SetAllocator(m_allocator);

			BlockMemory blocks { m_allocator };
			Map<JournalEntry> entries;
		
			if (ReadJournal(blocks, entries))
				if (entries.Any())
					ExecuteJournal(entries);

			m_journalFile.Clear();
		}

		m_state = State::Ready;
	}


	void AfsFileStorage::SetMaxSizeBytes(uint64 maxSizeBytes)
	{
		if (UINT64_MAX == maxSizeBytes)
			m_maxNrBlocks = UINT64_MAX;
		else
			m_maxNrBlocks = SatSub<uint64>(maxSizeBytes, MinBlockSize) / m_blockSize;
	}


	AfsResult::E AfsFileStorage::AddNewBlock(AfsBlock& block)
	{
		EnsureThrow(State::JournaledWrite == m_state);

		uint64 const nrBlocks = NrBlocks();
		if (nrBlocks >= m_maxNrBlocks)
			return AfsResult::OutOfSpace;

		uint64 const blockIndex = nrBlocks;
		Rp<RcBlock> dataBlock = new RcBlock { m_allocator };
		block.Init(*this, blockIndex, dataBlock);

		bool added {};
		m_blocksInUse.FindOrAdd(added, blockIndex);
		EnsureThrow(added);

		++m_nrBlocksToAdd;
		return AfsResult::OK;
	}


	AfsResult::E AfsFileStorage::ObtainBlock(AfsBlock& block, uint64 blockIndex)
	{
		if (State::Recoverable_ClearJournal   == m_state ||
		    State::Recoverable_ExecuteJournal == m_state)
			TryRecover();

		EnsureThrow(State::Ready == m_state || State::JournaledWrite == m_state);

		if (blockIndex >= m_nrBlocksStored)
			return AfsResult::BlockIndexInvalid;

		Rp<RcBlock>& cachedBlock = m_cachedBlocks.FindOrInsertEntry(blockIndex);
		if (cachedBlock.Any())
			++m_nrCacheHits;
		else
		{
			OnExit removeCacheEntry = [&] { m_cachedBlocks.RemoveEntries(blockIndex, blockIndex); };
			cachedBlock = new RcBlock { m_allocator };
			uint64 offset = MinBlockSize + (m_blockSize * blockIndex);
			m_dataFile.ReadBlocks(cachedBlock->Ptr(), 1, offset);
			removeCacheEntry.Dismiss();
			++m_nrCacheMisses;
		}

		if (State::JournaledWrite == m_state)
		{
			bool added {};
			m_blocksInUse.FindOrAdd(added, blockIndex);
			EnsureThrow(added);
		}

		block.Init(*this, blockIndex, cachedBlock);
		m_cachedBlocks.PruneEntries(m_cacheTargetSize, m_cacheMaxAge);
		return AfsResult::OK;
	}


	AfsResult::E AfsFileStorage::ObtainBlockForOverwrite(AfsBlock& block, uint64 blockIndex)
	{
		EnsureThrow(State::JournaledWrite == m_state);

		if (blockIndex >= m_nrBlocksStored)
			return AfsResult::BlockIndexInvalid;

		Rp<RcBlock> createdBlock;
		Rp<RcBlock>* dataBlock = m_cachedBlocks.FindEntry(blockIndex);
		if (dataBlock)
			++m_nrCacheHits;
		else
		{
			createdBlock = new RcBlock { m_allocator };
			dataBlock = &createdBlock;
			++m_nrCacheMisses;
		}

		bool added {};
		m_blocksInUse.FindOrAdd(added, blockIndex);
		EnsureThrow(added);

		block.Init(*this, blockIndex, *dataBlock);
		m_cachedBlocks.PruneEntries(m_cacheTargetSize, m_cacheMaxAge);
		return AfsResult::OK;
	}


	void AfsFileStorage::BeginJournaledWrite()
	{
		if (State::Recoverable_ClearJournal   == m_state ||
		    State::Recoverable_ExecuteJournal == m_state)
			TryRecover();

		EnsureThrow(State::Ready == m_state);
		m_state = State::JournaledWrite;
	}


	void AfsFileStorage::AbortJournaledWrite() noexcept
	{
		if (State::Recoverable_ClearJournal   == m_state ||
		    State::Recoverable_ExecuteJournal == m_state ||
			State::Unrecoverable              == m_state)
			return;

		EnsureThrow(State::JournaledWrite == m_state || State::Abortable == m_state);
		m_blocksInUse.Clear();
		m_nrBlocksToAdd = 0;
		m_state = State::Ready;
	}


	void AfsFileStorage::CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite)
	{
		EnsureThrow(State::JournaledWrite == m_state);
		uint64 const startNrBlocksStored = m_nrBlocksStored;
		uint64 const expectNewNrBlocksStored = m_nrBlocksStored + m_nrBlocksToAdd;
		sizet nrBlocksToAddWritten {};

		m_state = State::Unrecoverable;

		Map<JournalEntry> entries;
		for (Rp<AfsBlock> const& block : blocksToWrite)
		{
			uint64 const blockIndex = block->BlockIndex();

			EnsureThrow(block->ChangePending());
			JournalEntry e;
			e.m_blockIndex = blockIndex;
			e.m_block.Set(block->ReadPtr(), m_blockSize);

			bool added {};
			entries.FindOrAdd(added, std::move(e));
			EnsureThrow(added);

			if (blockIndex >= m_nrBlocksStored)
			{
				EnsureThrowWithNr2(blockIndex < expectNewNrBlocksStored, blockIndex, expectNewNrBlocksStored);
				++nrBlocksToAddWritten;
			}

			Rp<RcBlock>& cacheEntry = m_cachedBlocks.FindOrInsertEntry(blockIndex);
			cacheEntry = block->GetDataBlock();
		}

		EnsureThrowWithNr2(nrBlocksToAddWritten == m_nrBlocksToAdd, nrBlocksToAddWritten, m_nrBlocksToAdd);

		if (m_consistency < Consistency::Journal)
			ExecuteJournal(entries);
		else
		{
			// If an IO error occurs while writing the journal, we can let the journaled write be aborted by clearing the journal and cache.
			// If an additional IO error occurs while clearing the journal, we can go into state Recoverable_ClearJournal.
			// We do want to clear the journal here, if possible. If it can be cleared now, better to not defer until recovery, which may not come.
			try { WriteJournal(entries); }
			catch (StorageFile::IoErr const&)
			{
				try { m_journalFile.Clear(); }
				catch (StorageFile::IoErr const&)
				{ 
					m_state = State::Recoverable_ClearJournal;
					throw;
				}

				m_cachedBlocks.Clear();
				m_state = State::Abortable;
				throw;
			}

			// If an IO error occurs while executing the journal, we can recover if we can successfully execute the journal later.
			// In this case, we ignore the IO error, act as though this transaction completed, but set state to Recoverable_Xxxx.
			// However, if an exception other than an IO error occurs, then the state becomes Unrecoverable. In this case,
			// we must pass the exception through, but if we do that, code will act as though transaction was aborted when it was
			// already written to journal and will be committed on recovery
			try
			{
				if (m_consistency != Consistency::VerifyJournal)
					ExecuteJournal(entries);
				else
				{
					BlockMemory readBlocks { m_allocator };
					Map<JournalEntry> readEntries;
					ReadJournal(readBlocks, readEntries);
					ExecuteJournal(readEntries);
				}
			}
			catch (StorageFile::IoErr const&)
			{
				m_state = State::Recoverable_ExecuteJournal;
				return;
			}

			try
			{
				m_journalFile.Clear();
			}
			catch (StorageFile::IoErr const&)
			{
				m_state = State::Recoverable_ClearJournal;
				return;
			}
		}

		EnsureThrowWithNr2(expectNewNrBlocksStored == m_nrBlocksStored, expectNewNrBlocksStored, m_nrBlocksStored);
		m_nrBlocksToAdd = 0;
		m_blocksInUse.Clear();
		m_cachedBlocks.PruneEntries(m_cacheTargetSize, m_cacheMaxAge);
		m_state = State::Ready;
	}


	bool AfsFileStorage::ReadJournal(BlockMemory& blocks, Map<JournalEntry>& entries)
	{
		EnsureThrow(m_consistency >= Consistency::Journal);

		entries.Clear();

		uint64 journalSize64 = m_journalFile.GetSize();
		if (!journalSize64)
			return false;

		sizet journalSize = NumCast<sizet>(journalSize64);
		blocks.ReInit(journalSize);
		m_journalFile.ReadBlocks(blocks.Ptr(), blocks.NrBlocks(), 0);

		Seq reader { blocks.Ptr(), journalSize };
		byte* pAligned = blocks.Ptr();

		while (true)
		{
			JournalOp::E const op = (JournalOp::E) reader.ReadByte();
			if (JournalOp::End == op) return true;
			if (JournalOp::Entry != op) return false;

			JournalEntry e;
			if (!DecodeUInt64LE(reader, e.m_blockIndex)) return false;

			// We overwrite the read buffer as we go, putting block data into aligned locations
			Seq unaligned = reader.ReadBytes(m_blockSize);
			if (unaligned.n != m_blockSize) return false;
			EnsureThrow(pAligned < unaligned.p);
			memmove(pAligned, unaligned.p, m_blockSize);
			e.m_block.Set(pAligned, m_blockSize);
			pAligned += m_blockSize;

			entries.Add(std::move(e));
		}
	}


	void AfsFileStorage::WriteJournal(Map<JournalEntry> const& entries)
	{
		EnsureThrow(m_consistency >= Consistency::Journal);

		sizet const bytesPerEntry = 1 + 8 + m_blockSize;
		sizet const totalBytes = (bytesPerEntry * entries.Len()) + 1;
		BlockMemory blocks { m_allocator, totalBytes };
		byte* p = blocks.Ptr();
		byte const* const pEnd = p + totalBytes;

		for (JournalEntry const& e : entries)
		{
			EnsureThrow(p + bytesPerEntry < pEnd);
			EnsureThrow(e.m_block.n == m_blockSize);

			*p++ = JournalOp::Entry;
			p = EncodeUInt64LE_Ptr(p, e.m_blockIndex);
			memcpy(p, e.m_block.p, m_blockSize); p += m_blockSize;
			EnsureAbort(p < pEnd);
		}

		EnsureThrow(p + 1 == pEnd);
		*p++ = JournalOp::End;

		m_journalFile.WriteBlocks(blocks.Ptr(), blocks.NrBlocks(), 0);
		m_journalFile.SetEof(blocks.NrBlocks() * m_blockSize);
	}


	void AfsFileStorage::ExecuteJournal(Map<JournalEntry>& entries)
	{
		if (!entries.Any())
			return;

		Map<JournalEntry>::ConstIt itStart = entries.begin();
		Map<JournalEntry>::ConstIt it = entries.begin();
		sizet nrEntries = 1;

		while (true)
		{
			uint64 const expectBlockIndex = it->m_blockIndex + 1;
			++it;

			if (it.Any() && (it->m_blockIndex == expectBlockIndex))
				++nrEntries;
			else
			{
				ExecuteConsecutiveJournalEntries(itStart, it, nrEntries);
				if (!it.Any())
					break;

				itStart = it;
				nrEntries = 1;
			}
		}

		if (m_consistency == Consistency::Flush)
			if (!FlushFileBuffers(m_dataFile.Handle()))
				{ LastWinErr e; throw e.Make<StorageFile::IoErr>("FlushFileBuffers"); }

		uint64 const lastBlockIndex = entries.Last().m_blockIndex;
		if (lastBlockIndex >= m_nrBlocksStored)
			m_nrBlocksStored = lastBlockIndex + 1;
	}


	void AfsFileStorage::ExecuteConsecutiveJournalEntries(Map<JournalEntry>::ConstIt it, Map<JournalEntry>::ConstIt const& itEnd, sizet nrEntries)
	{
		EnsureThrow(it.Any());
		uint64 const firstBlockIndex = it->m_blockIndex;
		uint64 const fileOffset = MinBlockSize + (m_blockSize * firstBlockIndex);

		if (1 == nrEntries)
		{
			sizet ptrAsNr = (sizet) it->m_block.p;
			EnsureThrow(0 == (ptrAsNr % StorageFile::MinSectorSize));
			EnsureThrowWithNr2(it->m_block.n == m_blockSize, it->m_block.n, m_blockSize);

			m_dataFile.WriteBlocks(it->m_block.p, 1, fileOffset);
		}
		else
		{
			sizet const totalBytes = nrEntries * m_blockSize;
			BlockMemory blocks { m_allocator, totalBytes };
			byte* p = blocks.Ptr();
			byte const* const pEnd = p + totalBytes;

			uint64 expectBlockIndex = firstBlockIndex;
			while (it != itEnd)
			{
				EnsureThrow(p + m_blockSize <= pEnd);
				EnsureThrowWithNr2(it->m_blockIndex == expectBlockIndex, it->m_blockIndex, expectBlockIndex);
				EnsureThrowWithNr2(it->m_block.n == m_blockSize, it->m_block.n, m_blockSize);
			
				memcpy(p, it->m_block.p, m_blockSize);
				p += m_blockSize;

				++it;
				++expectBlockIndex;
			}

			EnsureThrow(p == pEnd);
			m_dataFile.WriteBlocks(blocks.Ptr(), blocks.NrBlocks(), fileOffset);
		}
	}


	void AfsFileStorage::TryRecover()
	{
		if (Consistency::Journal <= m_consistency)
		{
			if (State::Recoverable_ExecuteJournal == m_state)
			{

				BlockMemory blocks { m_allocator };
				Map<JournalEntry> entries;
		
				if (ReadJournal(blocks, entries))
				{
					if (entries.Any())
						ExecuteJournal(entries);

					m_state = State::Recoverable_ClearJournal;
				}
			}

			if (State::Recoverable_ClearJournal == m_state)
			{
				m_journalFile.Clear();
				m_cachedBlocks.Clear();

				m_nrBlocksToAdd = 0;
				m_blocksInUse.Clear();

				m_state = State::Ready;
			}
		}
	}

}
