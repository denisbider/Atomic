#include "AtIncludes.h"
#include "AtRwLock.h"

namespace At
{

	void RwLock::AcquireReadLock()
	{
		bool readLockAcquired {};
		do
		{
			if (InterlockedIncrement(&m_flag) < WRITE_LOCK_THRESHOLD)
				readLockAcquired = true;
			else
			{
				InterlockedDecrement(&m_flag);
				Sleep(0);
			}
		}
		while (!readLockAcquired);
	}

	
	void RwLock::AcquireWriteLock()
	{
		bool writeLockAcquired {};
		do
		{
			// Acquisition of write lock requires no current readers - m_flag must be zero
			if (InterlockedCompareExchange(&m_flag, WRITE_LOCK_THRESHOLD, 0) == 0)
				writeLockAcquired = true;
			else
				Sleep(0);
		}
		while (!writeLockAcquired);
	}

}
