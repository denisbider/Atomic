#pragma once

#include "AtEmailEntities.h"
#include "AtTextLog.h"


namespace At
{

	class SmtpSendLog
	{
	public:
		virtual void SmtpSendLog_OnReset   (RpVec<SmtpMsgToSend> const& msgs) = 0;
		virtual void SmtpSendLog_OnAttempt (SmtpMsgToSend const& msg, sizet contentLen) = 0;
		virtual void SmtpSendLog_OnResult  (SmtpMsgToSend const& msg, sizet contentLen, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) = 0;
	};



	class TextSmtpSendLog : public SmtpSendLog, public TextLog
	{
	public:
		void SmtpSendLog_OnReset   (RpVec<SmtpMsgToSend> const& msgs) override final;
		void SmtpSendLog_OnAttempt (SmtpMsgToSend const& msg, sizet contentLen) override final;
		void SmtpSendLog_OnResult  (SmtpMsgToSend const& msg, sizet contentLen, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) override final;

	private:
		// For contentLen, pass SIZE_MAX if unknown (e.g. because content not loaded when logging the particular event type)
		static void Enc_Msg(Enc& enc, SmtpMsgToSend const& msg, sizet contentLen);
	};

}
