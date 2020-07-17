#pragma once

#include "OgnIncludes.h"
#include "OgnEntities.h"


class OgnSmtpSender : public SmtpSender, public SmtpSendLog
{
public:
	// WorkPool interface

	void WorkPool_LogEvent(WORD eventType, Seq text) override final;

public:
	// SmtpSender interface

	Entity&      SmtpSender_GetStorageParent   ()                            override final;
	SmtpSendLog& SmtpSender_GetSendLog         ()                            override final;
	void         SmtpSender_GetCfg             (SmtpSenderCfg& cfg)    const override final;
	Str          SmtpSender_SenderComputerName (Seq fromDomainName)    const override final;
	void         SmtpSender_AddSchannelCerts   (Seq ourName, Schannel& conn) override final;

	SmtpDeliveryInstr::E SmtpSender_InTx_OnDeliveryResult(SmtpMsgToSend& msg, Seq msgContent,
		Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) override final;

	void SmtpSender_InTx_OnMsgRemoved(SmtpMsgToSend const& msg) override final;

public:
	// SmtpSendLog interface
	void SmtpSendLog_OnReset   (RpVec<SmtpMsgToSend> const& msgs) override final;
	void SmtpSendLog_OnAttempt (SmtpMsgToSend const& msg) override final;
	void SmtpSendLog_OnResult  (SmtpMsgToSend const& msg, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) override final;
};

