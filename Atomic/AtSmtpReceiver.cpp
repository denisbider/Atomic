#include "AtIncludes.h"
#include "AtSmtpReceiver.h"

#include "AtNumCvt.h"
#include "AtSmtpReceiverThread.h"
#include "AtWinErr.h"


namespace At
{

	void SmtpReceiver::SmtpReceiver_AddSchannelCerts(Schannel&)
		{ throw NotImplemented(); }

	EmailServerAuthResult SmtpReceiver::SmtpReceiver_Authenticate(SockAddr const&, Schannel&, Seq, EhloHost const&, Seq, Seq, Seq, Rp<SmtpReceiverAuthCx>&)
		{ throw NotImplemented(); }

	EntVec<EmailSrvBinding> const& SmtpReceiver::EmailServer_GetCfgBindings()
	{
		SmtpReceiver_GetCfg(m_cfg);
		return m_cfg.f_bindings;
	}

}
