#include "AtIncludes.h"
#include "AtWorkPoolThread.h"

#include "AtAuto.h"
#include "AtNumCvt.h"
#include "AtVec.h"
#include "AtWait.h"
#include "AtWinErr.h"


namespace At
{

	void WorkPoolThreadBase::BeforeThreadStart()
	{
		if (!m_workPoolBase)
			throw ZLitErr("AtWorkPoolThreadBase: WorkPool not set");

		InterlockedIncrement_PtrDiff(&(m_workPoolBase->m_nrThreads));
	}


	void WorkPoolThreadBase::OnThreadExit(ExitType::E)
	{
		InterlockedDecrement_PtrDiff(&(m_workPoolBase->m_nrThreads));
	}


	void WorkPoolThreadBase::ThreadMain()
	{
		try
		{
			m_workPoolBase->m_workAvailableEvent.ClearSignal();

			while (true)
			{
				// Attempt to grab work item from queue
				void* pvWorkItem {};

				{
					Locker locker { m_workPoolBase->m_mxWorkQueue };
					if (!m_workPoolBase->m_workQueue.empty())
					{
						pvWorkItem = m_workPoolBase->m_workQueue.front();
						m_workPoolBase->m_workQueue.pop_front();
					}
				}

				m_workPoolBase->m_workItemTakenEvent.Signal();

				// If we have a work item, process it
				if (pvWorkItem)
					WorkPoolThread_ProcessWorkItem(pvWorkItem);
				else
				{
					// Wait a reasonable time for another work item
					DWORD waitResult {};

					{
						InterlockedIncrement_PtrDiff(&(m_workPoolBase->m_nrThreadsReady));
						OnExit autoDecrement( [&] () { InterlockedDecrement_PtrDiff(&(m_workPoolBase->m_nrThreadsReady)); } );

						waitResult = Wait2(StopEvent().Handle(), m_workPoolBase->m_workAvailableEvent.Handle(), ReadyStateWaitMs);
					}

					if (waitResult != 1)
						break;
				}
			}
		}
		catch (ExecutionAborted const&)
		{
			InterlockedIncrement_PtrDiff(&(m_workPoolBase->m_nrThreadsAborted));
		}
		catch (std::exception const& e)
		{
			WorkPool_LogEvent(EVENTLOG_ERROR_TYPE, Str("Work pool thread stopped by exception: ").Add(e.what()));
			StopCtl().Stop("Error in work pool thread");
		}
		catch (...)
		{
			WorkPool_LogEvent(EVENTLOG_ERROR_TYPE, "Work pool thread stopped by unrecognized exception");
			StopCtl().Stop("Unrecognized error in work pool thread");
		}
	}

}
