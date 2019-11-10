#pragma once

#include "AtDescEnum.h"
#include "AtEntityStore.h"
#include "AtThread.h"


namespace At
{

	// Entities

	DESCENUM_DECL_BEGIN(BgTaskState)
	DESCENUM_DECL_VALUE(Unknown,      0)
	DESCENUM_DECL_VALUE(Starting,   100)
	DESCENUM_DECL_VALUE(InProgress, 200)
	DESCENUM_DECL_VALUE(Success,    300)
	DESCENUM_DECL_VALUE(Error,      400)
	DESCENUM_DECL_CLOSE()


	ENTITY_DECL_BEGIN(BgTasks)
	ENTITY_DECL_CLOSE()


	ENTITY_DECL_BEGIN(BgTaskInfo)
	ENTITY_DECL_FLD_K(Str,                      token, KeyCat::Key_Str_Unique_Sensitive)
	ENTITY_DECL_FIELD(Str,                      name)
	ENTITY_DECL_FLD_E(BgTaskState,              state)
	ENTITY_DECL_FIELD(Str,                      summary)
	ENTITY_DECL_FIELD(Vec<Str>,                 messages)
	ENTITY_DECL_FIELD(Str,                      onDoneUrl)
	ENTITY_DECL_CLOSE()



	// BgTask

	struct BgTask : Thread
	{
		EntityStore* m_store {};
		ObjId        m_taskInfoId;

	protected:
		void ThreadMain() override final;

		virtual void RunTask() = 0;
	};



	// BgTaskStarter

	template <class TaskType>
	struct BgTaskStarter
	{
		Rp<BgTaskInfo>      m_taskInfo;
		ThreadPtr<TaskType> m_task { Thread::Create };

		// Initialize any additional fields in m_task before calling Start()
		void Start(EntityStore& store, ObjId parentId, Seq name, Seq onDoneUrl, Rp<StopCtl> const& stopCtl)
		{
			m_taskInfo = new BgTaskInfo(store, parentId);
			m_taskInfo->f_token    = Token::Generate();
			m_taskInfo->f_name     = name;
			m_taskInfo->f_state    = BgTaskState::Starting;
			m_taskInfo->f_onDoneUrl = onDoneUrl;
			m_taskInfo->Insert_ParentLoaded();

			m_task->m_store      = &store;
			m_task->m_taskInfoId = m_taskInfo->m_entityId;

			ThreadPtr<TaskType> taskCopy = m_task;
			Rp<StopCtl> stopCtlCopy = stopCtl;
			store.AddPostCommitAction( [taskCopy, stopCtlCopy] { taskCopy->Start(stopCtlCopy); } );
		}

		// Call only after Start()
		Seq Token() const { return m_taskInfo->f_token; }
	};

}
