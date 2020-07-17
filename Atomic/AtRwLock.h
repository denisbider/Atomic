#pragma once

#include "AtIncludes.h"

namespace At
{

	class RwLock : NoCopy
	{
	public:
		// Unlike CRITICAL_SECTION, this lock cannot be acquired multiple times at the same time by the same thread.
		void AcquireReadLock() noexcept;
		void ReleaseReadLock() noexcept { InterlockedDecrement(&m_flag); }

		// Acquisition of write lock requires no current readers - not even the current thread.
		// Unlike CRITICAL_SECTION, this lock cannot be acquired multiple times at the same time by the same thread.
		void AcquireWriteLock() noexcept;

		void ReleaseWriteLock() noexcept
		{
			// There may be read lock attempts going on, so we have to subtract - not just set to zero
			InterlockedExchangeAdd(&m_flag, -WRITE_LOCK_THRESHOLD);
		}

	private:
		static LONG const WRITE_LOCK_THRESHOLD = LONG_MAX/4;
		LONG volatile m_flag {};
	};


	class ReadLocker : private NoCopy
	{
	public:
		ReadLocker   (RwLock& l) noexcept : m_l(&l) { l.AcquireReadLock(); }
		~ReadLocker  () noexcept                    { if (m_l) m_l->ReleaseReadLock(); }
		void Release () noexcept                    { m_l->ReleaseReadLock(); m_l = nullptr; }
		void Dismiss () noexcept                    { m_l = nullptr; }
	private:
		RwLock* m_l;
	};


	class WriteLocker : private NoCopy
	{
	public:
		WriteLocker  (RwLock& l) noexcept : m_l(&l) { l.AcquireWriteLock(); }
		~WriteLocker () noexcept                    { if (m_l) m_l->ReleaseWriteLock(); }
		void Release () noexcept                    { m_l->ReleaseWriteLock(); m_l = nullptr; }
		void Dismiss () noexcept                    { m_l = nullptr; }
	private:
		RwLock* m_l;
	};

}
