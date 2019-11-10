#pragma once

#include "AtWorkPool.h"


namespace At
{

	class WorkPoolThreadBase : virtual public Thread
	{
	public:
		enum { ReadyStateWaitMs = 15000 };

	protected:
		WorkPoolBase* m_workPoolBase {};

		void BeforeThreadStart();
		void ThreadMain();
		void OnThreadExit(ExitType::E exitType);

		// Must take ownership of the work item and free it when it's no longer needed.
		virtual void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) = 0;

		void WorkPool_LogEvent(WORD eventType, Seq text) { m_workPoolBase->WorkPool_LogEvent(eventType, text); }
	};


	template <class WorkPoolType>
	class WorkPoolThread : public WorkPoolThreadBase
	{
	public:
		void SetWorkPool(WorkPoolBase* workPool)
		{
			EnsureThrow(!Started());
			m_workPoolBase = workPool;
			m_workPool = dynamic_cast<WorkPoolType*>(workPool);
			EnsureThrow(m_workPool != nullptr);
		}

	protected:
		WorkPoolType* m_workPool {};
	};

}
