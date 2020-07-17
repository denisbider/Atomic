#pragma once

#include "AtException.h"
#include "AtStopCtl.h"
#include "AtWorkPool.h"


namespace At
{
	// SCManager

	class SCManager : public NoCopy
	{
	public:
		struct Err : public StrErr { Err(Seq msg) : StrErr(msg) {} };

		// Throws Err if cannot open service manager.
		// The service manager remains open until the object is destroyed.
		SCManager();
		~SCManager() { Close(); }

		// Checks if the service is registered, returns true if yes, false if no. No throw.
		bool IsRegistered(wchar_t const* service) const;

		SC_HANDLE Handle() { return m_scmHandle; }

		void Close() { if (m_scmHandle) CloseServiceHandle(m_scmHandle); m_scmHandle = 0; }

	private:
		SC_HANDLE m_scmHandle;
	};



	// Service

	class Service : public NoCopy
	{
	public:
		Service();
		~Service();
	
		static Service& GetInstance() { return *s_singleton; }
	
		// Does not return - calls ExitProcess() when done.
		// Service must be resilient to abrupt process termination, e.g. in case of power failure.
		// In order to ensure that such resilience exists, every process termination is abrupt.
		void  Main             (int argc, wchar_t const* const* argv);

		bool  IsRegistered     () const;
		void  Register         ();
		void  Unregister       ();

		DWORD Start            (int argc, wchar_t const* const* argv);
		bool  StartedAsService () const { return m_startedAsService; }
		void  SetState         (DWORD state) noexcept;
		void  Stop             (Seq reason);
	
		void  LogEvent         (WORD eventType, Seq text);

		void  LogError         (Seq text) { LogEvent(EVENTLOG_ERROR_TYPE,       text); }
		void  LogWarning       (Seq text) { LogEvent(EVENTLOG_WARNING_TYPE,     text); }
		void  LogInfo          (Seq text) { LogEvent(EVENTLOG_INFORMATION_TYPE, text); }

		Rp<StopCtl> const& GetStopCtl() { return m_stopCtl; }

		// Called by Run() to wait for work pools started by the implementation.
		// Waits for the service stop event to be signaled, then waits for all threads registered with StopCtl to stop.
		void WaitServiceStop(Seq initInfo);

	public:
		virtual wchar_t const* ServiceName() const = 0;
		virtual wchar_t const* ServiceDisplayName() const = 0;
		virtual wchar_t const* ServiceDescription() const = 0;
		virtual DWORD LogEventId() const = 0;
	
		// Should call WaitServiceStop() to enter ready state.
		virtual DWORD Run(int argc, wchar_t const* const* argv) = 0;

	private:
		bool                  m_startedAsService {};
		SERVICE_STATUS_HANDLE m_serviceStatusHandle {};
		SERVICE_STATUS        m_status;
		Rp<StopCtl>           m_stopCtl;

	public:
		void ServiceMain(DWORD argc, LPWSTR* argv);
		void ServiceCtrlHandler(DWORD opCode);

	private:
		static Service* s_singleton;

		void CallRun(int argc, wchar_t const* const* argv);
	};
}
