#include "AtIncludes.h"
#include "AtStopCtl.h"

#include "AtDllKernel32.h"
#include "AtTime.h"


namespace At
{

	void StopCtl::AddThread(Rp<ThreadInfo> const& ti)
	{
		Locker locker { m_mx };
		CleanUp_Inner();
		m_threads.Add(ti);
	}


	void StopCtl::Stop(Seq reason)
	{
		if (!m_stopEvent.IsSignaled())
		{
			Locker locker { m_mx };
			m_stopReason.Set(reason);
			m_stopEvent.Signal();
		}
	}


	bool StopCtl::WaitAll(DWORD ms)
	{
		ULONGLONG nowTicks {}, endTicks { UINT64_MAX };
		if (ms != INFINITE)
		{
			nowTicks = E_GetTickCount64();
			endTicks = SatAdd<ULONGLONG>(nowTicks, ms);
		}

		while (true)
		{
			Rp<ThreadInfo> ti;

			{
				Locker locker { m_mx };
				CleanUp_Inner();
				if (!m_threads.Any())
					return true;

				ti.Set(m_threads.Last());
			}

			if (ms == INFINITE)
				Wait1(ti->Handle(), INFINITE);
			else
			{
				nowTicks = E_GetTickCount64();
				if (nowTicks >= endTicks)
					return false;

				ULONGLONG waitTicks = endTicks - nowTicks;
				if (waitTicks > ms)
					waitTicks = ms;		// A way to guarantee (endTicks - nowTicks < MAXDWORD). Note that MAXDWORD also equals INFINITE
				Wait1(ti->Handle(), (DWORD) waitTicks);
			}
		}
	}


	void StopCtl::CleanUp_Inner()
	{
		for (sizet i=0; i!=m_threads.Len(); )
			if (Wait1(m_threads[i]->Handle(), 0) == 0)
				m_threads.Erase(i, 1);
			else
				++i;
					
	}

}
