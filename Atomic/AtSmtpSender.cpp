#include "AtIncludes.h"
#include "AtSmtpSender.h"

#include "AtInitOnFirstUse.h"
#include "AtNumCvt.h"
#include "AtSmtpSenderThread.h"
#include "AtTime.h"
#include "AtWait.h"


namespace At
{

	// SmtpSenderWorkItem

	SmtpSenderWorkItem::~SmtpSenderWorkItem() noexcept
	{
		if (m_memUsage.Any())
			InterlockedExchangeAdd_PtrDiff(&m_memUsage->m_nrBytes, -(NumCast<ptrdiff>(m_content.n)));
	}


	ptrdiff SmtpSenderWorkItem::RegisterMemUsage(Rp<SmtpSenderMemUsage> const& memUsage)
	{
		EnsureThrow(!m_memUsage.Any());
		m_memUsage = memUsage;
		ptrdiff const contentLen = NumCast<ptrdiff>(m_content.n);
		ptrdiff const v = InterlockedExchangeAdd_PtrDiff(&m_memUsage->m_nrBytes, contentLen);
		EnsureThrow(v >= 0);
		return v + contentLen;
	}



	// SmtpSender

	Rp<SmtpMsgToSend> SmtpSender::CreateMsg()
	{ 
		Rp<SmtpMsgToSend> msg = new SmtpMsgToSend(Entity::ChildOf, SmtpSender_GetStorageParent());

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

		// Common maximum email sizes range from 10 MB, most commonly 20 - 30 MB, up to 150 MB:
		// https://blog.mailtrap.io/email-size/#Limits_for_popular_email_sending_providers_and_clients_like_Gmail_Outlook_Sendgrid_etc
		//
		// The below allows up to 330 seconds to send a 30 MB message, with a minimum provisioned time of 90 seconds per message.

		msg->f_baseSendSecondsMax = 90;
		msg->f_nrBytesToAddOneSec = 128 * 1024;

		return msg;
	}


	bool SmtpSender::SendNextQueuedMessageNow()
	{
		Time nowPlusOne = Time::StrictNow();
		++nowPlusOne;

		Rp<SmtpMsgToSend> msgToSend;

		SmtpSender_GetStorageParent().FindChildren<SmtpMsgToSend>(nowPlusOne, nullptr,
			[&] (Rp<SmtpMsgToSend> const& msg) -> bool
			{
				if (msg->f_status == SmtpMsgStatus::NonFinal_Idle) { msgToSend = msg; return false; }
				return true;
			} );

		if (!msgToSend.Any())
			return false;

		msgToSend->f_nextAttemptTime = Time();
		msgToSend->Update();

		SignalTrigger();
		return true;
	}


	void SmtpSender::SmtpSender_InTx_LoadMoreContent(SmtpMsgToSend const&, Enc&)
	{
		throw NotImplemented();
	}


	void SmtpSender::WorkPool_Run()
	{
		m_memUsage = new SmtpSenderMemUsage;

		// Reset status on any messages that might be stuck in sending state from a previous run
		{
			RpVec<SmtpMsgToSend> msgsToReset;

			GetStore().RunTxExclusive( [&] ()
				{
					SmtpSender_GetStorageParent().EnumAllChildrenOfKind<SmtpMsgToSend>(
						[&] (Rp<SmtpMsgToSend> const& msg) -> bool
							{ if (msg->f_status == SmtpMsgStatus::NonFinal_Sending) msgsToReset.Add(msg); return true; } );

					if (msgsToReset.Any())
					{
						// Delay when we start resending messages from last run so that the admin has opportunity to take any emergency actions.
						// Don't start resending the messages all at once, but instead spread them out over a reasonable period of time.

						Time perMsgDelay = Time::FromMilliseconds(InitResumeSend_DefaultPerMsgDelayMs);
						sizet const nrDelays = msgsToReset.Len() - 1;
						Time const lastMsgDelay = perMsgDelay * nrDelays;
						Time const maxLastMsgDelay = Time::FromMinutes(InitResumeSend_MaxLastMsgDelayMins);
						if (nrDelays && lastMsgDelay > maxLastMsgDelay)
							perMsgDelay = maxLastMsgDelay / nrDelays;

						Time const now = Time::StrictNow();
						Time nextMsgTime = now + Time::FromMinutes(InitResumeSend_DelayMins);
		
						for (Rp<SmtpMsgToSend> const& msg : msgsToReset)
						{
							msg->f_nextAttemptTime = nextMsgTime;
							msg->f_status = SmtpMsgStatus::NonFinal_Idle;
							msg->Update();

							nextMsgTime += perMsgDelay;
						}
					}
				} );

			SmtpSender_GetSendLog().SmtpSendLog_OnReset(msgsToReset);
		}

		// Pump messages
		while (true)
		{
			// Check for messages to send
			SmtpSenderCfg cfg { Entity::Contained };
			SmtpSender_GetCfg(cfg);

			bool atMemUsageLimit {};
			Time nextPumpTime;

			if (0 != cfg.f_memUsageLimitKb)
			{
				ptrdiff const curUsageBytes = InterlockedExchangeAdd_PtrDiff(&m_memUsage->m_nrBytes, 0);
				if (curUsageBytes >= SatMulConst<ptrdiff, 1024>(SatCast<ptrdiff>(cfg.f_memUsageLimitKb)))
					atMemUsageLimit = true;
			}

			if (!atMemUsageLimit)
			{
				AutoFreeVec<SmtpSenderWorkItem> workItems;

				GetStore().RunTx(GetStopCtl(), typeid(*this), [&] ()
					{
						workItems.Clear();
						nextPumpTime = Time::Max();

						Time timeMin = Time::Min();
						Time timeNow = Time::StrictNow();
						Time timeNowPlusOne = timeNow;
						++timeNowPlusOne;

						SmtpSender_GetStorageParent().FindChildren<SmtpMsgToSend>(timeMin, &timeNowPlusOne,
							[&] (Rp<SmtpMsgToSend> const& m) -> bool
							{
								if (m->f_status == SmtpMsgStatus::NonFinal_Idle)
								{
									SmtpSenderWorkItem* wi = new SmtpSenderWorkItem;
									AutoFree<SmtpSenderWorkItem> autoFreeWorkItem { wi };
									workItems.Add(autoFreeWorkItem);

									wi->m_msg = m;
									wi->m_msg->f_status = SmtpMsgStatus::NonFinal_Sending;
									wi->m_msg->Update();

									if (!wi->m_msg->f_moreContentContext.Any())
										wi->m_content = wi->m_msg->f_contentPart1;
									else
									{
										wi->m_contentStorage = wi->m_msg->f_contentPart1;
										SmtpSender_InTx_LoadMoreContent(wi->m_msg.Ref(), wi->m_contentStorage);
										wi->m_content = wi->m_contentStorage;
									}

									LONG64 const curUsageBytes = wi->RegisterMemUsage(m_memUsage);
									if (0 != cfg.f_memUsageLimitKb)
										if (curUsageBytes >= SatMulConst<LONG64, 1024>(SatCast<LONG64>(cfg.f_memUsageLimitKb)))
										{
											atMemUsageLimit = true;
											return false;
										}
								}

								return true;
							} );

						SmtpSender_GetStorageParent().FindChildren<SmtpMsgToSend>(timeNowPlusOne, nullptr,
							[&] (Rp<SmtpMsgToSend> const& m) -> bool
								{ nextPumpTime = m->f_nextAttemptTime; return false; } );
					} );

				// Enqueue messages
				for (sizet i=0; i!=workItems.Len(); ++i)
				{
					AutoFree<SmtpSenderWorkItem> afwi;
					workItems.Extract(i, afwi);
					EnqueueWorkItem(afwi);
				}
			}

			// Wait for a trigger, stop signal, or next pump time
			DWORD waitMs = INFINITE;
			if (atMemUsageLimit)
				waitMs = AtMemUsageLimit_PumpDelayMs;
			else if (nextPumpTime != Time::Max())
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
