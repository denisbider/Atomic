#include "AtEmailEntities.h"
#include "AtSchannel.h"
#include "AtSocketConnection.h"
#include "AtSmtpSender.h"
#include "AtSocket.h"
#include "AtWorkPoolThread.h"


namespace At
{

	class SmtpSenderThread : public WorkPoolThread<SmtpSender>
	{
	private:
		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;

		void LookupRelayAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, Seq const content, bool contains8bit,
			Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

		void FindMailExchangersAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, Seq const content, bool contains8bit,
			Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

		void SendMsgToMailExchanger(SmtpSenderCfg const& cfg, Timeouts const& timeouts, Rp<SmtpSenderConnection> const& ssc,
			SmtpMsgToSend const& msg, Seq const content, bool contains8bit, Vec<MailboxResult>& mailboxResults);

		void PerformAuthPlain   (SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpSenderConnection& ssc, Rp<SmtpSendFailure> const& prevFailure);
		void PerformAuthCramMd5 (SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpSenderConnection& ssc, Rp<SmtpSendFailure> const& prevFailure);

		struct SendAttempter
		{
			Rp<SmtpSendFailure> m_firstConnFailure;
			Rp<SmtpSendFailure> m_firstTempFailure;
			Str                 m_failDescSuffix;
			sizet               m_nrTempDeliveryFailures {};
			bool                m_haveGoodEnoughResult   {};

			enum class Result { Stop, TryMore };
			Result TrySendMsgToMailExchanger(SmtpSenderThread& outer, SmtpSenderCfg const& cfg, Timeouts const& timeouts, Rp<SmtpSenderConnection> const& ssc,
				SmtpMsgToSend const& msg, Seq const content, bool contains8bit, Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

			void OnSuccessOrAttemptsExhausted();
		};
	};

}
