#include "AtIncludes.h"
#include "AtPageAllocator.h"

#include "AtException.h"
#include "AtWinErr.h"

namespace At
{
	// PageAllocator

	PageAllocator::PageAllocator()
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		m_pageSize = si.dwPageSize;
	}


	PageAllocator::~PageAllocator()
	{
		try
		{
			for (byte*& page : m_availPages)
			{
				FreeSequentialPages(page);
				page = nullptr;
			}
		}
		catch (Exception const& e)
		{
			EnsureFailWithDesc(OnFail::Report, e.what(), __FUNCTION__, __LINE__);
		}
	}


	void PageAllocator::SetMaxAvailPercent(uint maxAvailPercent)
	{
		if (maxAvailPercent > 100)
			maxAvailPercent = 100;

		m_maxAvailPercent = maxAvailPercent;

		FreeSuperfluousPages();
	}


	byte* PageAllocator::GetPage()
	{
		++m_pagesUsed;
		EnsureAbort(m_pagesUsed != 0);
		if (m_maxPagesUsed < m_pagesUsed)
			m_maxPagesUsed = m_pagesUsed;

		byte* p;
	
		if (!m_availPages.Any())
			p = AllocSequentialPages(1);
		else
		{
			p = m_availPages.Last();
			m_availPages.PopLast();
		}

		return p;
	}


	void PageAllocator::ReleasePage(void* p)
	{
		EnsureAbort(m_pagesUsed > 0);
		--m_pagesUsed;
	
		if (100 * m_availPages.Len() >= m_maxPagesUsed * m_maxAvailPercent)
			FreeSequentialPages(p);
		else
		{
			memset(p, 0, m_pageSize);
			m_availPages.Add((byte*) p);
		}
	}


	void PageAllocator::FreeSuperfluousPages()
	{
		EnsureAbort(m_availPages.Len() < (SIZE_MAX / 100));
		EnsureAbort(m_maxPagesUsed < (SIZE_MAX / 100));

		while (100 * m_availPages.Len() > m_maxPagesUsed * m_maxAvailPercent)
		{
			void* p = m_availPages.Last();
			m_availPages.PopLast();

			FreeSequentialPages(p);
		}
	}


	byte* PageAllocator::AllocSequentialPages(sizet nrPages)
	{
		void* p = VirtualAlloc(0, nrPages * m_pageSize, MEM_COMMIT, PAGE_READWRITE);
		if (!p)
			{ LastWinErr e; throw e.Make<>("AtPageAllocator: VirtualAlloc failed"); }
		return (byte*) p;
	}


	void PageAllocator::FreeSequentialPages(void* p)
	{
		if (!VirtualFree(p, 0, MEM_RELEASE))
			{ LastWinErr e; throw e.Make<>("AtPageAllocator: VirtualFree failed"); }
	}

}
