#include "AtDnsQuery.h"
#include "AtEmailEntities.h"
#include "AtSchannel.h"
#include "AtSmtpSender.h"
#include "AtSocket.h"
#include "AtWorkPoolThread.h"


namespace At
{

	class SmtpSenderThread : public WorkPoolThread<SmtpSender>
	{
	private:
		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;

		void LookupRelayAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, bool contains8bit,
			Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

		void FindMailExchangersAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, bool contains8bit,
			Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

		void SendMsgToMailExchanger(SmtpSenderCfg const& cfg, Timeouts const& timeouts, LookedUpAddr const& mxa, bool haveMxDomainMatch,
			SmtpMsgToSend const& msg, bool contains8bit, Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

		void PerformAuthPlain   (SmtpSenderCfg const& cfg, Timeouts const& timeouts, LookedUpAddr const& mxa, Schannel& conn);
		void PerformAuthCramMd5 (SmtpSenderCfg const& cfg, Timeouts const& timeouts, LookedUpAddr const& mxa, Schannel& conn);

		struct SendAttempter
		{
			Str                 m_firstConnFailure;
			LookedUpAddr        m_firstConnFailureMxa    {};
			Rp<SmtpSendFailure> m_firstTempFailure;
			Str                 m_failDescSuffix;
			sizet               m_nrTempDeliveryFailures {};
			bool                m_haveGoodEnoughResult   {};

			enum class Result { Stop, TryMore };
			Result TrySendMsgToMailExchanger(SmtpSenderThread& outer, SmtpSenderCfg const& cfg, Timeouts const& timeouts, LookedUpAddr const& mxa, bool haveMxDomainMatch,
				SmtpMsgToSend const& msg, bool contains8bit, Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved);

			void OnSuccessOrAttemptsExhausted();
		};
	};

}
