#pragma once

#include "AtEvent.h"
#include "AtRpAnchor.h"


namespace At
{

	// Intended usage situation for InitWaitEvent:
	// - Multiple threads may need to access a resource that requires costly initialization.
	// - Initialization may take an unreasonable amount of time and may fail.
	// - The thread performing initialization needs to signal other threads when it is done.
	// - Other threads need to pay attention to a stop event in order to be responsive to shutdown.
	// - The resources that may require such initialization are many and need separate wait events.
	// - Since events are kernel objects, we don't want to create them unnecessarily.

	class InitWaitEvent
	{
	public:
		// Returns whether initialization is completed at this moment. Does not wait.
		bool Inited() const noexcept { return m_inited; }

		// Returns true if wait completed, false if timeout expired, throws ExecutionAborted if stopCtl.StopEvent() is signaled.
		// "waitMs" can be INFINITE for no timeout.
		bool WaitInit(StopCtl& stopCtl, DWORD waitMs);

		// Signals to any waiting threads that initialization has completed.
		void SignalInited() noexcept;

	private:
		RpAnchor<Rc<Event>>   m_evtInit;			// Event is created on-demand only if any threads need to wait for it
		ptrdiff volatile      m_nrWaiting {};
		bool volatile         m_inited {};
	};

}
