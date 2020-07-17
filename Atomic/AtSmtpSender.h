#pragma once

#include "AtBCrypt.h"
#include "AtEntityStore.h"
#include "AtSchannel.h"
#include "AtSmtpSendLog.h"
#include "AtWorkPool.h"


namespace At
{

	enum
	{
		Smtp_AttemptMaxTempFailures = 3,
		Smtp_MaxServerReplyBytes = 32000,
	};



	struct SmtpDeliveryInstr { enum E { Abort, KeepTrying }; };


	class SmtpSenderThread;

	struct SmtpSenderMemUsage : RefCountable
	{
		ptrdiff volatile m_nrBytes {};
	};

	struct SmtpSenderWorkItem
	{
		Rp<SmtpMsgToSend> m_msg;
		Str m_contentStorage;
		Seq m_content;

		Rp<SmtpSenderMemUsage> m_memUsage;

		~SmtpSenderWorkItem() noexcept;

		// Returns total mem usage bytes after registering the current work item
		ptrdiff RegisterMemUsage(Rp<SmtpSenderMemUsage> const& memUsage);
	};

	class SmtpSender : public WorkPool<SmtpSenderThread, SmtpSenderWorkItem>
	{
	public:
		// Creates a SmtpMsgToSend so that the caller can populate it with information.
		// Once populated, the message should be passed to Send.
		Rp<SmtpMsgToSend> CreateMsg();

		// Sends a message, using SMTP, to the specified address.
		// - Must be called from within an active Db transaction.
		// - Single delivery domain per call, but multiple mailboxes OK.
		// - Message is sent as-is, all headers must already be part of content, no headers are added.
		// - The method returns immediately after enqueueing the message for delivery.
		//   On delivery success or failure, the SmtpSender_OnDeliveryResult method is called.
		void Send(Rp<SmtpMsgToSend> const& msg) { msg->Insert_ParentExists(); msg->GetStore().AddPostCommitAction( [this] () { m_pumpTrigger.Signal(); } ); }

		// Signal SmtpSender to run its main loop if the message queue has changed - e.g. next attempt time for a message has been set to send now.
		void SignalTrigger() { GetStore().AddPostCommitAction( [this] () { m_pumpTrigger.Signal(); } ); }

		EntityStore& GetStore() { return SmtpSender_GetStorageParent().GetStore(); }

	protected:
		// Must return the database entity under which the SMTP sender's data is to be stored.
		virtual Entity& SmtpSender_GetStorageParent() = 0;
		virtual SmtpSendLog& SmtpSender_GetSendLog() = 0;

		// Called separately by each sender thread. Must copy or initialize SMTP sender configuration into the provided entity.
		virtual void SmtpSender_GetCfg(SmtpSenderCfg& cfg) const = 0;

		// A fully qualified sender computer name that will be sent as part of the EHLO command.
		virtual Str SmtpSender_SenderComputerName(Seq fromDomainName) const = 0;

		// Called by sender threads each time TLS is started
		virtual void SmtpSender_AddSchannelCerts(Seq ourName, Schannel&) = 0;

		// Called before enqueueing the message if msg.f_moreContentContext is non-empty
		virtual void SmtpSender_InTx_LoadMoreContent(SmtpMsgToSend const& msg, Enc& enc);

		// - Called on a different thread than the corresponding call to Send. Even likely called in an entirely separate process instance than the original Send.
		// - Pointers do not persist across process instances, therefore delivery context is a string.
		// - SmtpDeliveryInstruction is ignored if delivery to all mailboxes succeeded or failed permanently. Retries will occur only for mailboxes that failed temporarily.
		// - There will be no further retries if: there are no temporary failures; or this function returns SmtpDeliveryInstr::Abort; or if msg.f_futureRetryDelayMinutes is empty.
		// - Called with SmtpMsgToSend not yet modified: the fields f_status, f_pendingMailboxes, f_mailboxResults are not yet updated.
		//   These fields are updated if the message is being retried, after the function returns. If the message is not being retried, it is removed.
		// - Called in the same EntityStore transaction in which SmtpMsgToSend will be updated or removed. If the transaction is repeated, this function is also called again.
		virtual SmtpDeliveryInstr::E SmtpSender_InTx_OnDeliveryResult(SmtpMsgToSend& msg, Seq msgContent,
			Vec<MailboxResult> const& newMailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) = 0;

		// Called after removing a message. When called, the message is already removed and msg.f_status is set to one of SmtpMsgStatus indicating type of removal.
		virtual void SmtpSender_InTx_OnMsgRemoved(SmtpMsgToSend const& msg) = 0;

	private:
		enum { AtMemUsageLimit_PumpDelayMs = 100 };
		Event m_pumpTrigger { Event::CreateAuto };
		Rp<SmtpSenderMemUsage> m_memUsage;

		LONG volatile    m_md5ProviderInitFlag {};
		BCrypt::Provider m_md5Provider;

		void WorkPool_Run() override;
		BCrypt::Provider const& GetMd5Provider();

		friend class SmtpSenderThread;
	};

}
