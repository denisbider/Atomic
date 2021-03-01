#pragma once

#include "AtAfs.h"
#include "AtMap.h"


namespace At
{

	class AfsMemStorage : public AfsStorage
	{
	public:
		AfsMemStorage(uint32 blockSize, uint64 maxNrBlocks) : m_blockSize(blockSize), m_maxNrBlocks(maxNrBlocks)
			{ m_allocator.SetBytesPerBlock(blockSize); }

		uint32 BlockSize() override final { return m_blockSize; }
		BlockAllocator& Allocator() override final { return m_allocator; }
		uint64 MaxNrBlocks() override final { return m_maxNrBlocks; }
		uint64 NrBlocks() override final { return m_blocks.Len(); }

		AfsResult::E AddNewBlock(AfsBlock& block) override final;
		AfsResult::E ObtainBlock(AfsBlock& block, uint64 blockIndex) override final;
		void BeginJournaledWrite() override final;
		void AbortJournaledWrite() noexcept override final;
		void CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite) override final;

	private:
		uint32 const   m_blockSize;
		BlockAllocator m_allocator;
		uint64 const   m_maxNrBlocks;

		struct StoredBlock
		{
			uint64      m_blockIndex { UINT64_MAX };
			Rp<RcBlock> m_data;

			uint64 Key() const { return m_blockIndex; }
		};

		Map<StoredBlock> m_blocks;
		uint64 m_nextBlockIndex {};

		bool m_haveJournaledWrite {};
		OrderedSet<uint64> m_obtainedBlocks;
		Vec<StoredBlock> m_addBlocks;
		uint64 m_addNextBlockIndex {};
	};

}
