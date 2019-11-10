#pragma once

#include "AtEvent.h"
#include "AtMutex.h"
#include "AtRcHandle.h"
#include "AtRpVec.h"
#include "AtWait.h"


namespace At
{

	struct ThreadInfo : RcHandle
	{
		Str m_threadDesc;
	};


	class StopCtl : public RefCountable, NoCopy
	{
	public:
		Event& StopEvent  () { return m_stopEvent; }
		Seq    StopReason () { EnsureAbort(m_stopEvent.IsSignaled()); return m_stopReason; }

		void   AddThread  (Rp<ThreadInfo> const& ti);
		void   Stop       (Seq reason);
		bool   WaitAll    (DWORD ms);					// Returns true if all stopped, false if timeout
		void   WaitAll    () { EnsureThrow(WaitAll(INFINITE)); }

	private:
		Mutex             m_mx;
		Str               m_stopReason;
		Event             m_stopEvent { Event::CreateManual };
		RpVec<ThreadInfo> m_threads;

		void CleanUp_Inner();
	};

}
