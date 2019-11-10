#include "AtIncludes.h"
#include "AtBulkAlloc.h"

#include "AtMem.h"


namespace At
{
	BulkStorage::~BulkStorage()
	{
		BulkStorageBlock* block = m_first;
		while (block)
		{
			BulkStorageBlock* next = block->next;
			VirtualFree(block, BulkStorageBlock::BlockSize, MEM_RELEASE);
			block = next;
		}
	}


	byte* BulkStorage::Allocate(sizet size)
	{
		sizet alignedSize = ((size + 7) / 8) * 8;

		if (alignedSize > BulkStorageBlock::MaxAlloc)
			Mem::BadAlloc();
	
		if (!m_last || m_last->remainingNr < alignedSize)
		{
			BulkStorageBlock* block = (BulkStorageBlock*) VirtualAlloc(0, BulkStorageBlock::BlockSize, MEM_COMMIT, PAGE_READWRITE);
			if (!block)
				Mem::BadAlloc();
		
			sizet blockHeaderSize = ((sizeof(BulkStorageBlock) + 7) / 8) * 8;
			block->next = nullptr;
			block->remainingPtr = ((byte*) block) + blockHeaderSize;
			block->remainingNr = BulkStorageBlock::BlockSize - blockHeaderSize;
		
			if (!m_first)
				m_first = block;

			if (m_last)
				m_last->next = block;

			m_last = block;
		}

		byte* result = m_last->remainingPtr;
		m_last->remainingPtr += alignedSize;
		m_last->remainingNr -= alignedSize;
		return result;
	}
}
