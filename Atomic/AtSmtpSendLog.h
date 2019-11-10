#pragma once

#include "AtEmailEntities.h"
#include "AtTextLog.h"


namespace At
{

	class SmtpSendLog
	{
	public:
		virtual void SmtpSendLog_OnReset   (RpVec<SmtpMsgToSend> const& msgs) = 0;
		virtual void SmtpSendLog_OnAttempt (SmtpMsgToSend const& msg) = 0;
		virtual void SmtpSendLog_OnResult  (SmtpMsgToSend const& msg, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) = 0;
	};



	class TextSmtpSendLog : public SmtpSendLog, public TextLog
	{
	public:
		void SmtpSendLog_OnReset   (RpVec<SmtpMsgToSend> const& msgs) override final;
		void SmtpSendLog_OnAttempt (SmtpMsgToSend const& msg) override final;
		void SmtpSendLog_OnResult  (SmtpMsgToSend const& msg, Vec<MailboxResult> const& mailboxResults, SmtpTlsAssurance::E tlsAssuranceAchieved) override final;

	private:
		static void Enc_Msg(Enc& enc, SmtpMsgToSend const& msg);
	};

}
