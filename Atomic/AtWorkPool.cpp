#include "AtIncludes.h"
#include "AtWorkPool.h"

#include "AtWinErr.h"


namespace At
{

	void WorkPoolBase::ThreadMain()
	{
		try { WorkPool_Run(); }
		catch (ExecutionAborted const&) {}
		catch (std::exception const& e) { WorkPool_LogEvent(EVENTLOG_ERROR_TYPE, Str("Stopped by exception: ").Add(e.what())); }

		m_stopCtl->Stop(ThreadDesc().Add(" stopped"));

		while (true)
		{
			if (InterlockedExchangeAdd_PtrDiff(&m_nrThreads, 0) == 0)
				break;
		
			Sleep(100);
		}
	}

}
