#include "AtIncludes.h"
#include "AtBgTask.h"


namespace At
{

	// Entities

	DESCENUM_DEF_BEGIN(BgTaskState)
	DESCENUM_DEF_VALUE(Unknown)
	DESCENUM_DEF_VALUE(Starting)
	DESCENUM_DEF_VAL_X(InProgress, "In progress")
	DESCENUM_DEF_VALUE(Success)
	DESCENUM_DEF_VALUE(Error)
	DESCENUM_DEF_CLOSE()


	ENTITY_DEF_BEGIN(BgTasks)
	ENTITY_DEF_CLOSE(BgTasks)


	ENTITY_DEF_BEGIN(BgTaskInfo)
	ENTITY_DEF_FIELD(BgTaskInfo, token)
	ENTITY_DEF_FIELD(BgTaskInfo, name)
	ENTITY_DEF_FLD_E(BgTaskInfo, state)
	ENTITY_DEF_FIELD(BgTaskInfo, summary)
	ENTITY_DEF_FIELD(BgTaskInfo, messages)
	ENTITY_DEF_FIELD(BgTaskInfo, onDoneUrl)
	ENTITY_DEF_CLOSE(BgTaskInfo)



	// BgTask

	void BgTask::ThreadMain()
	{
		EnsureThrow(m_store);

		bool haveError {};

		try
		{
			RunTask();
		}
		catch (std::exception const& e)
		{
			haveError = true;

			m_store->RunTx(GetStopCtl(), typeid(BgTask), [&]
				{
					Rp<BgTaskInfo> taskInfo { m_store->GetEntityOfKind<BgTaskInfo>(m_taskInfoId, ObjId::None) };
					if (taskInfo.Any())
					{
						taskInfo->f_state = BgTaskState::Error;
						taskInfo->f_summary.SetAdd("Task terminated by exception:\r\n", e.what());
						taskInfo->Update();
					}
				} );
		}

		if (!haveError)
			m_store->RunTx(GetStopCtl(), typeid(BgTask), [&]
				{
					Rp<BgTaskInfo> taskInfo { m_store->GetEntityOfKind<BgTaskInfo>(m_taskInfoId, ObjId::None) };
					if (taskInfo.Any() && taskInfo->f_state != BgTaskState::Error)
					{
						taskInfo->f_state = BgTaskState::Success;
						taskInfo->Update();
					}
				} );
	}

}
