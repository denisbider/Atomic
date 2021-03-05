#include "AtIncludes.h"
#include "AtBlockAllocator.h"

#include "AtException.h"
#include "AtWinErr.h"


namespace At
{

	// BlockAllocator

	BlockAllocator::BlockAllocator()
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		m_bytesPerPage = si.dwPageSize;
		m_bytesPerBlock = m_bytesPerPage * m_pagesPerBlock;
	}


	BlockAllocator::~BlockAllocator()
	{
		FreeAvailBlocks();
		EnsureReportWithNr(0 == m_blocksUsed, m_blocksUsed);
	}


	void BlockAllocator::SetPagesPerBlock(sizet nrPages)
	{
		EnsureThrow(0 != nrPages);
		EnsureThrowWithNr(0 == m_blocksUsed, m_blocksUsed);

		if (nrPages != m_pagesPerBlock)
		{
			FreeAvailBlocks();
			m_pagesPerBlock = nrPages;
			m_bytesPerBlock = m_bytesPerPage * m_pagesPerBlock;
		}
	}


	void BlockAllocator::SetMaxAvailPercent(uint maxAvailPercent)
	{
		if (maxAvailPercent > 100)
			maxAvailPercent = 100;

		m_maxAvailPercent = maxAvailPercent;

		FreeSuperfluousBlocks();
	}


	byte* BlockAllocator::GetBlock()
	{
		++m_blocksUsed;
		EnsureAbort(m_blocksUsed != 0);
		if (m_maxBlocksUsed < m_blocksUsed)
			m_maxBlocksUsed = m_blocksUsed;

		byte* p;
	
		if (!m_availBlocks.Any())
		{
			p = AllocMemory(m_bytesPerBlock);
			++m_nrCacheMisses;
		}
		else
		{
			p = m_availBlocks.Last();
			m_availBlocks.PopLast();
			++m_nrCacheHits;
		}

		return p;
	}


	void BlockAllocator::ReleaseBlock(void* p) noexcept
	{
		EnsureAbort(m_blocksUsed > 0);
		--m_blocksUsed;
	
		if (HaveSuperfluousBlocks())
			FreeMemory(p);
		else
		{
			// We go to all this trouble so that the function can be noexcept, which is important in destructors
			try
			{
				bool badAlloc {};
				try { m_availBlocks.ReserveInc(1); }
				catch (std::bad_alloc const&) { badAlloc = true; }

				if (badAlloc)
					FreeMemory(p);
				else
				{
					memset(p, 0, m_bytesPerBlock);
					m_availBlocks.Add((byte*) p);
				}
			}
			catch (std::exception const& e)
			{
				Str msg = Str::Join("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n", Seq::WithNull(e.what()));
				EnsureFail_Abort(msg.CharPtr());
			}
		}
	}


	bool BlockAllocator::HaveSuperfluousBlocks() const noexcept
	{
		if (m_availBlocks.Len() <= MinBlocksToCache)
			return false;

		sizet const onePercentOfSizeMax = SIZE_MAX / 100;
		if (m_availBlocks.Len() > onePercentOfSizeMax)
			return true;

		sizet maxBlocks = PickMin<sizet>(m_maxBlocksUsed, onePercentOfSizeMax);
		return 100 * m_availBlocks.Len() >= maxBlocks * m_maxAvailPercent;
	}


	void BlockAllocator::FreeSuperfluousBlocks()
	{
		while (HaveSuperfluousBlocks())
		{
			void* p = m_availBlocks.Last();
			m_availBlocks.PopLast();
			FreeMemory(p);
		}
	}


	byte* BlockAllocator::AllocMemory(sizet nrBytes)
	{
		void* p = VirtualAlloc(0, nrBytes, MEM_COMMIT, PAGE_READWRITE);
		if (!p)
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": Error in VirtualAlloc"); }
		return (byte*) p;
	}


	void BlockAllocator::FreeMemory(void* p) noexcept
	{
		if (!VirtualFree(p, 0, MEM_RELEASE))
		{
			DWORD err = GetLastError();
			EnsureFailWithNr(OnFail::Report, __FUNCTION__ ": Error in VirtualFree", err);
		}
	}


	void BlockAllocator::FreeAvailBlocks() noexcept
	{
		while (m_availBlocks.Any())
		{
			FreeMemory(m_availBlocks.Last());
			m_availBlocks.PopLast();
		}
	}



	// BlockMemory

	void BlockMemory::Init_Inner(sizet nrBytes)
	{
		m_nrBlocks = m_allocator.NrBlocksForBytes(nrBytes);
		m_p = m_allocator.AllocMemory(m_nrBlocks * m_allocator.BytesPerBlock());
	}

}
