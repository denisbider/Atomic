#pragma once

#include "AtIncludes.h"

#include "AtRp.h"
#include "AtSeq.h"
#include "AtVec.h"


namespace At
{

	class BlockAllocator : NoCopy
	{
	public:
		BlockAllocator();
		~BlockAllocator();

		sizet BytesPerPage  () const { return m_bytesPerPage; }
		sizet PagesPerBlock () const { return m_pagesPerBlock; }
		sizet BytesPerBlock () const { return m_bytesPerPage * m_pagesPerBlock; }

		sizet NrPagesForBytes  (sizet nrBytes) { return (nrBytes + m_bytesPerPage  - 1) / m_bytesPerPage;  }
		sizet NrBlocksForBytes (sizet nrBytes) { return (nrBytes + m_bytesPerBlock - 1) / m_bytesPerBlock; }

		// If not called, default block size is 1 page. MAY be called to change block size after use
		// If called after use, there must be NO currently outstanding blocks. Any cached blocks of previous size are freed
		void SetBytesPerBlock(sizet nrBytes) { SetPagesPerBlock(NrPagesForBytes(nrBytes)); }
		void SetPagesPerBlock(sizet nrPages);

		// The GetBlock() and ReleaseBlock() functions keep track of the number of blocks outstanding at any time,
		// and the maximum number of blocks that have ever been acquired and not yet released.
		// This function sets the number of blocks that will be kept around after they are released,
		// as a proportion of the maximum recorded number of outstanding blocks.
		void SetMaxAvailPercent(uint maxAvailPercent);

		byte* GetBlock();
		void ReleaseBlock(void* p);

		// Consumers that need a specific amount of memory can call AllocMemory and FreeMemory directly
		byte* AllocMemory(sizet nrBytes);
		void FreeMemory(void* p) noexcept;

		uint64 NrCacheHits   () const { return m_nrCacheHits; }
		uint64 NrCacheMisses () const { return m_nrCacheMisses; }

	private:
		sizet  m_bytesPerPage    {};
		sizet  m_pagesPerBlock   { 1 };
		sizet  m_bytesPerBlock   {};

		uint   m_maxAvailPercent { 25 };
		sizet  m_blocksUsed      {};
		sizet  m_maxBlocksUsed   {};
		uint64 m_nrCacheHits     {};
		uint64 m_nrCacheMisses   {};

		Vec<byte*> m_availBlocks;

		enum { MinBlocksToCache = 100 };

		bool HaveSuperfluousBlocks() const;
		void FreeSuperfluousBlocks();
		void FreeAvailBlocks() noexcept;
	};



	class AutoBlock : NoCopy
	{
	public:
		AutoBlock(BlockAllocator& allocator) : m_allocator(allocator), m_block(allocator.GetBlock()) {}
		~AutoBlock() { m_allocator.ReleaseBlock(m_block); }

		byte* Ptr() { return m_block; }

	private:
		BlockAllocator& m_allocator;
		byte* m_block;
	};

	using RcBlock = Rc<AutoBlock>;



	class BlockMemory : NoCopy
	{
	public:
		BlockMemory(BlockAllocator& allocator) : m_allocator(allocator) {}
		BlockMemory(BlockAllocator& allocator, sizet nrBytes) : m_allocator(allocator) { Init_Inner(nrBytes); }

		~BlockMemory() noexcept { Clear(); }

		void Clear() noexcept { if (m_p) { m_allocator.FreeMemory(m_p); m_p = nullptr; } }
		void ReInit(sizet nrBytes) { Clear(); Init_Inner(nrBytes); }

		sizet NrBlocks() const { return m_nrBlocks; }

		byte* Ptr() { return m_p; }
		byte const* Ptr() const { return m_p; }

	protected:
		BlockAllocator& m_allocator;
		sizet m_nrBlocks {};
		byte* m_p        {};

		void Init_Inner(sizet nrBytes);
	};

}
