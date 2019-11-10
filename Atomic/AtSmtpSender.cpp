#include "AtIncludes.h"
#include "AtSmtpSender.h"

#include "AtInitOnFirstUse.h"
#include "AtNumCvt.h"
#include "AtSmtpSenderThread.h"
#include "AtTime.h"
#include "AtWait.h"


namespace At
{

	Rp<SmtpMsgToSend> SmtpSender::CreateMsg()
	{ 
		Rp<SmtpMsgToSend> msg { new SmtpMsgToSend(Entity::ChildOf, SmtpSender_GetStorageParent()) };

		// This retry schedule is intended to cause messages sent toward the end of a business day to fail
		// (if they fail) by a time earlier in the next business day, giving time to look into the issue

		msg->f_futureRetryDelayMinutes.Add( 10U);	// attempt  2 after   10 minutes
		msg->f_futureRetryDelayMinutes.Add( 15U);	// attempt  3 after   25 minutes
		msg->f_futureRetryDelayMinutes.Add( 25U);	// attempt  4 after   50 minutes
		msg->f_futureRetryDelayMinutes.Add( 35U);	// attempt  5 after   85 minutes
		msg->f_futureRetryDelayMinutes.Add( 50U);	// attempt  6 after  135 minutes
		msg->f_futureRetryDelayMinutes.Add( 75U);	// attempt  7 after  210 minutes
		msg->f_futureRetryDelayMinutes.Add(115U);	// attempt  8 after  325 minutes
		msg->f_futureRetryDelayMinutes.Add(170U);	// attempt  9 after  495 minutes
		msg->f_futureRetryDelayMinutes.Add(215U);	// attempt 10 after  710 minutes
		msg->f_futureRetryDelayMinutes.Add(215U);	// attempt 11 after  925 minutes
		msg->f_futureRetryDelayMinutes.Add(215U);	// attempt 12 after 1140 minutes = 19 hours total
													// totals assuming each attempt is short - it may not be
		return msg;
	}


	void SmtpSender::WorkPool_Run()
	{
		// Reset status on any messages that might be stuck in sending state from a previous run
		{
			RpVec<SmtpMsgToSend> msgsToReset;

			GetStore().RunTxExclusive( [&] ()
				{
					SmtpSender_GetStorageParent().EnumAllChildrenOfKind<SmtpMsgToSend>(
						[&] (Rp<SmtpMsgToSend> const& msg) -> bool
							{ if (msg->f_status == SmtpMsgStatus::NonFinal_Sending) msgsToReset.Add(msg); return true; } );
		
					for (Rp<SmtpMsgToSend> const& msg : msgsToReset)
					{
						msg->f_status = SmtpMsgStatus::NonFinal_Idle;
						msg->Update();
					}
				} );

			SmtpSender_GetSendLog().SmtpSendLog_OnReset(msgsToReset);
		}

		// Pump messages
		while (true)
		{
			// Check for messages to send
			RpVec<SmtpMsgToSend> msgs;
			Time nextPumpTime;

			GetStore().RunTx(GetStopCtl(), typeid(*this), [&] ()
				{
					msgs.Clear();
					nextPumpTime = Time::Max();

					Time timeMin { Time::Min() };
					Time timeNow { Time::StrictNow() };

					SmtpSender_GetStorageParent().FindChildren<SmtpMsgToSend>(timeMin, &timeNow,
						[&] (Rp<SmtpMsgToSend> const& m) -> bool
							{ if (m->f_status == SmtpMsgStatus::NonFinal_Idle) msgs.Add(m); return true; } );

					for (Rp<SmtpMsgToSend> const& msg : msgs)
					{
						msg->f_status = SmtpMsgStatus::NonFinal_Sending;
						msg->Update();
					}

					SmtpSender_GetStorageParent().FindChildren<SmtpMsgToSend>(timeNow, nullptr,
						[&] (Rp<SmtpMsgToSend> const& m) -> bool
							{ nextPumpTime = m->f_nextAttemptTime; return false; } );
				} );

			// Enqueue messages
			for (Rp<SmtpMsgToSend> const& msg : msgs)
			{
				AutoFree<SmtpSenderWorkItem> workItem { new SmtpSenderWorkItem };
				workItem->m_msg = msg;

				EnqueueWorkItem(workItem);
			}

			// Wait for a trigger, stop signal, or next pump time
			DWORD waitMs { INFINITE };
			if (nextPumpTime != Time::Max())
				waitMs = SatCast<DWORD>((nextPumpTime - Time::StrictNow()).ToMilliseconds());

			if (Wait2(StopEvent().Handle(), m_pumpTrigger.Handle(), waitMs) == 0)
				break;
		}
	}


	BCrypt::Provider const& SmtpSender::GetMd5Provider()
	{
		InitOnFirstUse(&m_md5ProviderInitFlag, [this]
			{ m_md5Provider.OpenMd5(); } );

		return m_md5Provider;
	}

}
