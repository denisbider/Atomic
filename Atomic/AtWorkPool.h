#pragma once

#include "AtAuto.h"
#include "AtEvent.h"
#include "AtLogEvent.h"
#include "AtMutex.h"
#include "AtThread.h"
#include "AtWait.h"


namespace At
{

	class WorkPoolBase : virtual public Thread
	{
	public:
		// If not called, default value is used. If called, must be called before work pool is started.
		void SetMaxNrThreads(uint maxNrThreads) { EnsureThrow(!Started()); m_maxNrThreads = maxNrThreads; }

	private:
		void ThreadMain();

	protected:
		Event             m_workAvailableEvent { Event::CreateAuto };
		Event             m_workItemTakenEvent { Event::CreateAuto };
		uint              m_maxNrThreads       { 100 };

		ptrdiff volatile  m_nrThreads          {};
		ptrdiff volatile  m_nrThreadsReady     {};

		Mutex             m_mxWorkQueue;
		std::deque<void*> m_workQueue;		// Destroyed in WorkPool<> derived template

		virtual void WorkPool_Run() = 0;
		virtual void WorkPool_LogEvent(WORD eventType, Seq text) = 0;
	
		friend class WorkPoolThreadBase;
	};



	template <class ThreadType, class WorkItemType>
	class WorkPool : virtual public WorkPoolBase
	{
	public:
		virtual ~WorkPool()
		{
			while (!m_workQueue.empty())
			{
				delete (WorkItemType*) m_workQueue.front();
				m_workQueue.pop_front();
			}
		}

		void EnqueueWorkItem(AutoFree<WorkItemType>& workItem)
		{
			while (true)
			{
				{
					Locker locker(m_mxWorkQueue);
					if (m_workQueue.size() < m_maxNrThreads)
					{
						m_workQueue.push_back(nullptr);
						m_workQueue.back() = workItem.Dismiss();
						break;
					}
				}

				if (Wait2(StopEvent().Handle(), m_workItemTakenEvent.Handle(), INFINITE) == 0)
					throw ExecutionAborted();
			}

			if (InterlockedExchangeAdd_PtrDiff(&m_nrThreadsReady, 0) < 1 &&
				InterlockedExchangeAdd_PtrDiff(&m_nrThreads,      0) < SatCast<ptrdiff>(m_maxNrThreads))
			{
				ThreadPtr<ThreadType> thread { Thread::Create };
				thread->SetWorkPool(this);
				thread->Start(GetStopCtl());
			}

			m_workAvailableEvent.Signal();
		}
	};

}
