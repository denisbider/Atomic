#include "OgnIncludes.h"
#include "OgnSmtpSender.h"

#include "OgnConversions.h"
#include "OgnGlobals.h"



// WorkPool interface

void OgnSmtpSender::WorkPool_LogEvent(WORD eventType, Seq text)
{
	Rp<OgnStoredServiceSettings> settings = g_ognServiceSettings.Get();
	settings->m_logEvent(settings->m_callbackCx, eventType, OgnSeq_FromSeq(text));
}



// OgnSmtpSender: SmtpSender interface

Entity& OgnSmtpSender::SmtpSender_GetStorageParent()
{
	return g_ognSmtpSenderStorage.Ref();
}


SmtpSendLog& OgnSmtpSender::SmtpSender_GetSendLog()
{
	return *this;
}


void OgnSmtpSender::SmtpSender_GetCfg(SmtpSenderCfg& cfg) const
{
	cfg = g_ognSmtpCfg.Get()->m_smtpSenderCfg;
}


Str OgnSmtpSender::SmtpSender_SenderComputerName(Seq /*fromDomainName*/) const
{
	return g_ognSmtpCfg.Get()->m_senderComputerName;
}


void OgnSmtpSender::SmtpSender_AddSchannelCerts(Schannel& /*conn*/)
{
	// Not using certificates for SMTP client authentication
}


SmtpDeliveryInstr::E OgnSmtpSender::SmtpSender_InTx_OnDeliveryResult(SmtpMsgToSend&, Seq, Vec<MailboxResult> const&, SmtpTlsAssurance::E)
{
	return SmtpDeliveryInstr::KeepTrying;
}


void OgnSmtpSender::SmtpSender_InTx_OnMsgRemoved(SmtpMsgToSend const&)
{
	// Nothing to do.
}




// OgnSmtpSender: SmtpSendLog interface

void OgnSmtpSender::SmtpSendLog_OnReset(RpVec<SmtpMsgToSend> const& msgs)
{
	Vec<OgnMsgToSend> ognMsgs;
	Vec<OgnMsgStorage> storage;

	ognMsgs.ReserveExact(msgs.Len());
	storage.ReserveExact(msgs.Len());

	for (Rp<SmtpMsgToSend> const& msg : msgs)
		OgnMsgToSend_FromSmtp(ognMsgs.Add(), storage.Add(), msg.Ref());

	Rp<OgnStoredServiceSettings> settings = g_ognServiceSettings.Get();
	settings->m_onReset(settings->m_callbackCx, ognMsgs.Len(), ognMsgs.Ptr());
}


void OgnSmtpSender::SmtpSendLog_OnAttempt(SmtpMsgToSend const& msg)
{
	OgnMsgToSend ognMsg;
	OgnMsgStorage storage;

	OgnMsgToSend_FromSmtp(ognMsg, storage, msg);

	Rp<OgnStoredServiceSettings> settings = g_ognServiceSettings.Get();
	settings->m_onAttempt(settings->m_callbackCx, ognMsg);
}


void OgnSmtpSender::SmtpSendLog_OnResult(SmtpMsgToSend const& msg, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved)
{
	OgnMsgToSend ognMsg;
	OgnMsgStorage ognMsgStorage;

	OgnMsgToSend_FromSmtp(ognMsg, ognMsgStorage, msg);

	Vec<OgnMbxResult> ognMbxResults;
	Vec<OgnMbxResultStorage> ognMbxResultsStorage;

	OgnMbxResults_FromSmtp(ognMbxResults, ognMbxResultsStorage, mailboxResults);

	Rp<OgnStoredServiceSettings> settings = g_ognServiceSettings.Get();
	settings->m_onResult(settings->m_callbackCx, ognMsg, ognMbxResults.Len(), ognMbxResults.Ptr(), (OgnTlsAssurance::E) tlsAssuranceAchieved);
}
