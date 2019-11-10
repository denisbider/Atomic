#pragma once

#include "AtIncludes.h"
#include "AtSeq.h"
#include "AtVec.h"


namespace At
{

	class PageAllocator : NoCopy
	{
	public:
		PageAllocator();
		~PageAllocator();

		sizet PageSize() const { return m_pageSize; }
		sizet NrPagesForBytes(sizet nrBytes) { return (nrBytes + m_pageSize - 1) / m_pageSize; }
		void SetMaxAvailPercent(uint maxAvailPercent);
		byte* GetPage();
		void ReleasePage(void* p);

		// Consumers that need more than one page at a time can call AllocSequentialPages and FreeSequentialPages directly
		byte* AllocSequentialPages(sizet nrPages);
		void FreeSequentialPages(void* p);

	private:
		sizet m_pageSize {};

		uint m_maxAvailPercent { 25 };
		sizet m_pagesUsed {};
		sizet m_maxPagesUsed {};

		Vec<byte*> m_availPages;

		void FreeSuperfluousPages();
	};



	class AutoPage : NoCopy
	{
	public:
		AutoPage(PageAllocator& allocator) : m_allocator(allocator), m_page(allocator.GetPage()) {}
		~AutoPage() { m_allocator.ReleasePage(m_page); }

		byte* Ptr() { return m_page; }

	private:
		PageAllocator& m_allocator;
		byte* m_page;
	};



	class SequentialPages : NoCopy
	{
	public:
		SequentialPages(PageAllocator& allocator, sizet nrBytes) : m_allocator(allocator), m_nrPages(allocator.NrPagesForBytes(nrBytes))
			{ m_firstPage = allocator.AllocSequentialPages(m_nrPages); }

		~SequentialPages() { m_allocator.FreeSequentialPages(m_firstPage); }

		sizet Nr() const { return m_nrPages; }

		byte* Ptr() { return m_firstPage; }
		byte const* Ptr() const { return m_firstPage; }

	protected:
		PageAllocator& m_allocator;
		sizet m_nrPages;
		byte* m_firstPage;
	};

}
