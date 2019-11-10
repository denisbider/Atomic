#pragma once

#include "AtRcHandle.h"
#include "AtWebServer.h"
#include "AtWinStr.h"
#include "AtWorkPool.h"


namespace At
{

	class BhtpnServerThread;

	struct BhtpnServerWorkItem
	{
		Rp<RcHandle> m_sh;
	};

	class BhtpnServer : public WebServer<BhtpnServerThread, BhtpnServerWorkItem>
	{
	public:
		void GenPipeName();
		void GrantAccessToProcessOwner(bool v) { m_grantAccessToProcessOwner = v; }

		Seq GetPipeName() const { return m_pipeName; }

	private:
		bool m_grantAccessToProcessOwner {};

		Str m_sddlStr;
		Str m_pipeName;
		WinStr m_wsFullPipeName;

		Event m_pipeEvent { Event::CreateAuto };
		OVERLAPPED m_ovl;

		void WorkPool_Run() override final;

	protected:
		// Does not need to be thread-safe.
		// On exception, the web server work pool will exit.
		// If the exception derives from std::exception, the exception description will be logged.
		virtual char const* BhtpnServer_LogNamePrefix() const = 0;
	};

}
