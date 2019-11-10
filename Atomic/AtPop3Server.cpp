#include "AtIncludes.h"
#include "AtPop3Server.h"

#include "AtNumCvt.h"
#include "AtWinErr.h"


namespace At
{

	EntVec<EmailSrvBinding> const& Pop3Server::EmailServer_GetCfgBindings()
	{
		Pop3Server_GetCfg(m_cfg);
		return m_cfg.f_bindings;
	}

}
