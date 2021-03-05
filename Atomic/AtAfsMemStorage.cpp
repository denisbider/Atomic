#include "AtIncludes.h"
#include "AtAfsMemStorage.h"


namespace At
{

	AfsResult::E AfsMemStorage::AddNewBlock(AfsBlock& block)
	{
		EnsureThrow(m_haveJournaledWrite);
		if (m_maxNrBlocks == m_addNextBlockIndex)
			return AfsResult::OutOfSpace;

		StoredBlock& sb = m_addBlocks.Add();
		sb.m_blockIndex = m_addNextBlockIndex++;
		sb.m_data = new RcBlock { m_allocator };

		block.Init(*this, sb.m_blockIndex, sb.m_data);
		return AfsResult::OK;
	}


	AfsResult::E AfsMemStorage::ObtainBlock(AfsBlock& block, uint64 blockIndex)
	{
		if (blockIndex >= m_nextBlockIndex)
			return AfsResult::BlockIndexInvalid;

		if (m_haveJournaledWrite)
		{
			bool added {};
			OrderedSet<uint64>::It it = m_obtainedBlocks.FindOrAdd(added, blockIndex);
			EnsureThrow(it.Any());
			EnsureThrow(added);
		}

		Map<StoredBlock>::It it = m_blocks.Find(blockIndex);
		EnsureThrow(it.Any());

		StoredBlock& sb = it.Ref();
		block.Init(*this, sb.m_blockIndex, sb.m_data);
		return AfsResult::OK;
	}


	AfsResult::E AfsMemStorage::ObtainBlockForOverwrite(AfsBlock& block, uint64 blockIndex)
	{
		EnsureThrow(m_haveJournaledWrite);
		if (blockIndex >= m_nextBlockIndex)
			return AfsResult::BlockIndexInvalid;

		bool added {};
		OrderedSet<uint64>::It it = m_obtainedBlocks.FindOrAdd(added, blockIndex);
		EnsureThrow(it.Any());
		EnsureThrow(added);

		block.Init(*this, blockIndex, new RcBlock { m_allocator } );
		return AfsResult::OK;
	}


	void AfsMemStorage::BeginJournaledWrite()
	{
		EnsureThrow(!m_haveJournaledWrite);
		EnsureThrow(!m_addBlocks.Any());
		m_obtainedBlocks.Clear();
		m_addNextBlockIndex = m_nextBlockIndex;
		m_haveJournaledWrite = true;
	}


	void AfsMemStorage::AbortJournaledWrite() noexcept
	{
		EnsureThrow(m_haveJournaledWrite);
		m_addBlocks.Clear();
		m_obtainedBlocks.Clear();
		m_haveJournaledWrite = false;
	}


	void AfsMemStorage::CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite)
	{
		EnsureThrow(m_haveJournaledWrite);
		OrderedSet<uint64> blocksWritten;

		for (Rp<AfsBlock> const& pBlock : blocksToWrite)
		{
			AfsBlock& block = pBlock.Ref();
			EnsureThrow(block.Inited());
			EnsureThrow(block.ChangePending());

			uint64 blockIndex = block.BlockIndex();
			bool added {};
			blocksWritten.FindOrAdd(added, blockIndex);
			EnsureThrow(added);
			
			if (blockIndex < m_nextBlockIndex)
			{
				// Block existed before the current journaled write
				Map<StoredBlock>::It it = m_blocks.Find(blockIndex);
				EnsureThrow(it.Any());
				it->m_data = block.GetDataBlock();
			}
			else
			{
				// Block was added in the current journaled write
				EnsureThrow(blockIndex < m_addNextBlockIndex);

				sizet const addBlocksIndex = NumCast<sizet>(blockIndex - m_nextBlockIndex);
				StoredBlock& sb = m_addBlocks[addBlocksIndex];
				EnsureThrowWithNr2(sb.m_blockIndex == blockIndex, sb.m_blockIndex, blockIndex);
				sb.m_data = block.GetDataBlock();

				m_blocks.FindOrAdd(added, std::move(sb));
				EnsureThrow(added);
				sb.m_blockIndex = UINT64_MAX;
			}
		}

		// Verify that all blocks added in the current journaled write are being written
		for (sizet i=0; i!=m_addBlocks.Len(); ++i)
			EnsureThrowWithNr2(UINT64_MAX == m_addBlocks[i].m_blockIndex, i, m_addBlocks[i].m_blockIndex);

		m_nextBlockIndex = m_addNextBlockIndex;
		m_addBlocks.Clear();
		m_obtainedBlocks.Clear();
		m_haveJournaledWrite = false;
	}

}
