#include "AtIncludes.h"
#include "AtBhtpnServer.h"

#include "AtBhtpn.h"
#include "AtBhtpnServerThread.h"
#include "AtWinSecurity.h"


namespace At
{

	void BhtpnServer::GenPipeName()
	{
		m_pipeName = Gen_ModulePathBased_BhtpnPipeName();
	}


	void BhtpnServer::WorkPool_Run()
	{
		EnsureThrow(m_pipeName.Any());

		// Init log
		m_requestLog.Init(Str(BhtpnServer_LogNamePrefix()).Add("BhtpnLog"), TextLog::Flags::AutoRollover);

		// Init pipe name
		m_sddlStr = 
			"D:P"					// DACL, SE_DACL_PROTECTED
			"(A;;FRFW;;;BA)"		// Allow, no flags, FILE_GENERIC_READ | FILE_GENERIC_WRITE, Built-in Administrators
			"(A;;FRFW;;;SY)";		// Allow, no flags, FILE_GENERIC_READ | FILE_GENERIC_WRITE, Local System

		if (m_grantAccessToProcessOwner)
		{
			// Allow access to pipe by current process owner SID, so that elevation is not required for debugging
			TokenInfoOfProcess<TokenInfo_Owner> owner(GetCurrentProcess());
			StringSid ownerStringSid(owner.Ptr()->Owner);
			m_sddlStr.Add("(A;;FRFW;;;").Add(ownerStringSid.GetStr()).Add(")");
		}

		m_wsFullPipeName.Set(Str("\\\\.\\pipe\\").Add(m_pipeName));

		SecDescriptor sd(m_sddlStr);
		SecAttrs sa(sd, InheritHandle::No);

		// Create pipe instances and accept connections
		bool firstInstance = false;
		while (true)
		{
			if (StopEvent().IsSignaled())
				break;

			// Create pipe instance
			DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
			if (firstInstance)
			{
				openMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
				firstInstance = false;
			}

			AutoFree<BhtpnServerWorkItem> workItem = new BhtpnServerWorkItem;
			workItem->m_sh.Set(new RcHandle);
			workItem->m_sh->SetHandle(CreateNamedPipeW(m_wsFullPipeName.Z(), openMode, PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 0, 0, 0, sa.Ptr()));
			if (workItem->m_sh->Handle() == INVALID_HANDLE_VALUE)
				{ LastWinErr e; throw e.Make<>("BhtpnServer: Error in CreateNamedPipe()"); }

			ZeroMemory(&m_ovl, sizeof(m_ovl));
			m_ovl.hEvent = m_pipeEvent.Handle();

			// Wait for connection
			DWORD connectResult;
			if (ConnectNamedPipe(workItem->m_sh->Handle(), &m_ovl))
				connectResult = ERROR_PIPE_CONNECTED;
			else
			{
				connectResult = GetLastError();

				if (connectResult == ERROR_IO_PENDING)
				{
					if (Wait2(StopEvent().Handle(), m_pipeEvent.Handle(), INFINITE) == 0)
						break;

					DWORD bytesTransferred = 0;
					if (GetOverlappedResult(workItem->m_sh->Handle(), &m_ovl, &bytesTransferred, true))
						connectResult = ERROR_PIPE_CONNECTED;
					else
						connectResult = GetLastError();
				}
			}

			// Enqueue pipe
			EnqueueWorkItem(workItem);
		}
	}

}
