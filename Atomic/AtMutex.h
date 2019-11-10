#pragma once

#include "AtIncludes.h"

namespace At
{
	class Mutex : public NoCopy
	{
	public:
		Mutex() throw() { InitializeCriticalSection(&m_cs); }
		~Mutex() throw() { DeleteCriticalSection(&m_cs); }

		void Lock() throw() { EnterCriticalSection(&m_cs); }
		bool TryLock() throw() { return !!TryEnterCriticalSection(&m_cs); }
		void Unlock() throw() { LeaveCriticalSection(&m_cs); }

	private:
		CRITICAL_SECTION m_cs;
	};



	class Locker : public NoCopy
	{
	public:
		Locker(Mutex& mx) throw() : m_mx(mx) { m_mx.Lock(); }
		~Locker() throw() { m_mx.Unlock(); }

	protected:
		Mutex& m_mx;
	};
}
