#include "AtIncludes.h"
#include "AtInitWaitEvent.h"


namespace At
{

	bool InitWaitEvent::WaitInit(StopCtl& stopCtl, DWORD waitMs)
	{
		if (!m_inited)
		{
			InterlockedIncrement_PtrDiff(&m_nrWaiting);
			Rp<Rc<Event>> evt = m_evtInit.GetOrInit( [] (Rp<Rc<Event>>& x) { x = new Rc<Event> { Event::CreateManual }; } );
			OnExit onExit = [this] { if (!InterlockedDecrement_PtrDiff(&m_nrWaiting)) m_evtInit.Clear(); };

			if (!m_inited)
			{
				DWORD waitResult = Wait2(stopCtl.StopEvent().Handle(), evt->Handle(), waitMs);
				if (WAIT_TIMEOUT == waitResult) return false;
				if (0 == waitResult) throw ExecutionAborted();
				EnsureThrow(1 == waitResult);
				EnsureThrow(m_inited);
			}
		}

		return true;
	}


	void InitWaitEvent::SignalInited() noexcept
	{
		m_inited = true;
		Rp<Rc<Event>> evt = m_evtInit.Get();
		if (evt.Any())
			evt->Signal();
	}

}
