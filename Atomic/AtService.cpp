#include "AtIncludes.h"
#include "AtService.h"

#include "AtArgs.h"
#include "AtConsole.h"
#include "AtEntityStore.h"
#include "AtException.h"
#include "AtLogEvent.h"
#include "AtMutex.h"
#include "AtNumCvt.h"
#include "AtPath.h"
#include "AtSeTranslate.h"
#include "AtThread.h"
#include "AtWaitEsc.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	// SCManager

	SCManager::SCManager()
	{
		m_scmHandle = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
		if (!m_scmHandle)
			{ LastWinErr e; throw e.Make<Err>("Error in OpenSCManager"); }
	}


	bool SCManager::IsRegistered(wchar_t const* serviceName) const
	{
		SC_HANDLE srvHandle = OpenServiceW(m_scmHandle, serviceName, SERVICE_QUERY_CONFIG);
		if (srvHandle)
		{
			CloseServiceHandle(srvHandle);
			return true;
		}

		return false;
	}



	// Service

	Service* Service::s_singleton = nullptr;


	Service::Service()
	{
		if (s_singleton)
			throw ZLitErr("There is already another instance of Service");

		s_singleton = this;

		m_stopCtl.Set(new StopCtl);

		m_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		m_status.dwCurrentState = SERVICE_STOPPED;
		m_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN;
		m_status.dwWin32ExitCode = 0;
		m_status.dwServiceSpecificExitCode = 0;
		m_status.dwCheckPoint = 0;
		m_status.dwWaitHint = 0;
	}


	Service::~Service()
	{
		s_singleton = nullptr;
	}


	void Service::Main(int argc, wchar_t const* const* argv)
	{
		UINT exitCode = 0;
		_set_se_translator(SeTranslator);
	
		try
		{
			if (argc > 2)
				throw UsageErr("Invalid number of command line parameters");

			enum class Command { None, Register, Unregister };
			Command command = Command::None;

			if (argc > 1)
			{
				Args args { argc, argv };
				Seq cmd = args.Skip(1).Next();

				     if (cmd.EqualInsensitive("register"   )) { if (args.Any()) throw UsageErr("Unexpected additional parameters for 'register'"   ); command = Command::Register;   }
				else if (cmd.EqualInsensitive("unregister" )) { if (args.Any()) throw UsageErr("Unexpected additional parameters for 'unregister'" ); command = Command::Unregister; }
			}
		
			if (command == Command::Register)
			{
				Register();
				Console::Out("Service registered\r\n");
			}
			else if (command == Command::Unregister)
			{
				Unregister();
				Console::Out("Service unregistered\r\n");
			}
			else
				exitCode = Start(argc, argv);
		}
		catch (UsageErr const& e)
		{
			Console::Out(Str(e.what()).Add("\r\n"
							 "\r\n"
							 "Usage:\r\n"
							 "- Run without parameters to start service in console\r\n"
							 "- Use parameter 'register' or 'unregister' to register/unregister service\r\n"));
			
			exitCode = 2;
		}
		catch (std::exception const& e)
		{
			Console::Out(Str("Program terminated by exception:\r\n")
						.Add(e.what()).Add("\r\n"));

			exitCode = 1;
		}

		if (!StartedAsService())
		{
			// If we were started as service, console may already be closed at this point, and trying to write to it will throw
			Console::FlushOut();

			if (IsDebuggerPresent())
			{
				Console::Out(Str("\r\n"
								 "Program stopped with exit code ").UInt(exitCode).Add(". Press any key to close\r\n"));

				Console::FlushOut();
				Console::ReadChar();
			}
		}
	
		ExitProcess(exitCode);
	}


	bool Service::IsRegistered() const
	{
		SCManager scm;
		return scm.IsRegistered(ServiceName());
	}


	void Service::Register()
	{
		wchar_t const* serviceName = ServiceName();
		wchar_t const* serviceDisplayName = ServiceDisplayName();

		HKEY appLogKey;
		LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application", 0, KEY_CREATE_SUB_KEY, &appLogKey);
		if (rc != ERROR_SUCCESS)
			throw WinErr<>(rc, "Error opening Application log registry key");
		OnExit closeAppLogKey( [&] () { RegCloseKey(appLogKey); } );
	
		HKEY serviceLogKey;
		rc = RegCreateKeyExW(appLogKey, serviceName, 0, 0, 0, KEY_SET_VALUE, 0, &serviceLogKey, 0);
		if (rc != ERROR_SUCCESS)
			throw WinErr<>(rc, "Error creating event source registry key");
		OnExit closeServiceLogKey( [&] () { RegCloseKey(serviceLogKey); } );

		Str    modulePathN { GetModulePath() };
		WinStr modulePathW { modulePathN };
		rc = RegSetValueExW(serviceLogKey, L"EventMessageFile", 0, REG_SZ, (BYTE const*) modulePathW.Z(), NumCast<DWORD>(sizeof(wchar_t) * (modulePathW.Len() + 1)));
		if (rc != ERROR_SUCCESS)
			throw WinErr<>(rc, "Error writing event source registry key value 'EventMessageFile'");

		DWORD typesSupported { 7 };
		rc = RegSetValueExW(serviceLogKey, L"TypesSupported", 0, REG_DWORD, (BYTE const*) &typesSupported, sizeof(DWORD));
		if (rc != ERROR_SUCCESS)
			throw WinErr<>(rc, "Error writing event source registry key value 'TypesSupported'");

		Str modulePathQuotedN = Str::Join("\"", modulePathN, "\"");
		WinStr modulePathQuotedW { modulePathQuotedN };

		SCManager scm;
		SC_HANDLE serviceHandle = OpenServiceW(scm.Handle(), serviceName, SERVICE_CHANGE_CONFIG);
		OnExit closeServiceHandle( [&] () { if (serviceHandle != 0) CloseServiceHandle(serviceHandle); } );

		if (serviceHandle)
		{
			if (!ChangeServiceConfigW(serviceHandle, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
					SERVICE_ERROR_NORMAL, modulePathQuotedW.Z(), 0, 0, L"RpcSs\0TcpIp\0", 0, 0, serviceDisplayName))
				{ LastWinErr e; throw e.Make<>("Error in ChangeServiceConfig"); }
		}
		else
		{
			serviceHandle = CreateServiceW(scm.Handle(), serviceName, serviceDisplayName,
				SERVICE_ALL_ACCESS,	SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
				modulePathQuotedW.Z(), 0, 0, L"RpcSs\0TcpIp\0", 0, 0);

			if (!serviceHandle)
				{ LastWinErr e; throw e.Make<>("Error in CreateService"); }
		}

		WinStr desc { ServiceDescription() };
		SERVICE_DESCRIPTION sd;
		sd.lpDescription = (LPWSTR) desc.Z();
		if (!ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_DESCRIPTION, &sd))
			{ LastWinErr e; throw e.Make<>("Error in ChangeServiceConfig2"); }
	}


	void Service::Unregister()
	{
		wchar_t const* serviceName = ServiceName();

		SCManager scm;
		if (scm.IsRegistered(serviceName))
		{
			// Delete event log keys.
			{
				HKEY appLogKey;
				LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application", 0, DELETE, &appLogKey);
				if (rc != ERROR_SUCCESS)
					{ LastWinErr e; throw e.Make<>("Error opening Application log registry key"); }

				OnExit closeAppLogKey( [&] () { RegCloseKey(appLogKey); } );

				rc = RegDeleteKeyW(appLogKey, serviceName);
				if (rc != ERROR_FILE_NOT_FOUND && rc != ERROR_SUCCESS)
					{ LastWinErr e; throw e.Make<>("Error deleting event source registry key"); }
			}

			// Delete service.
			{
				SC_HANDLE serviceHandle = OpenService(scm.Handle(), serviceName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
				if (!serviceHandle)
				{
					DWORD lastErr = GetLastError();
					if (lastErr == ERROR_SERVICE_DOES_NOT_EXIST)
						return;
					throw WinErr<>(lastErr, "Error in OpenService");
				}
				OnExit closeServiceHandle( [&] () { CloseServiceHandle(serviceHandle); } );
			
				SERVICE_STATUS status;
				ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status);

				while (status.dwCurrentState == SERVICE_STOP_PENDING)
				{
					Sleep(100);
					if (!QueryServiceStatus(serviceHandle, &status))
						{ LastWinErr e; throw e.Make<>("Error in QueryServiceStatus"); }
				}

				if (!DeleteService(serviceHandle))
				{
					if (GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE)
						{ LastWinErr e; throw e.Make<>("Error in DeleteService"); }
					return;
				}
			}

			// Test if service is marked for delete.
			{
				Sleep(200);

				SC_HANDLE serviceHandle = OpenService(scm.Handle(), serviceName, DELETE);
				if (serviceHandle)
				{
					OnExit closeServiceHandle( [&] () { CloseServiceHandle(serviceHandle); } );
					if (!DeleteService(serviceHandle))
						if (GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE)
							return;
				}
			}
		}

		return;
	}


	static void WINAPI CallWinServiceMain(DWORD argc, LPWSTR* argv)
	{
		Service::GetInstance().ServiceMain(argc, argv);
	}


	DWORD Service::Start(int argc, wchar_t const* const* argv)
	{
		SetErrorMode(SEM_FAILCRITICALERRORS);

		wchar_t const* serviceName = ServiceName();
	
		SERVICE_TABLE_ENTRYW ste[] =
		{
			{ (LPWSTR) serviceName, CallWinServiceMain },
			{ 0, 0 }
		};
	
		if (!IsDebuggerPresent() && StartServiceCtrlDispatcher(ste))
		{
			// Service was started by service control dispatcher, and at this point it has already shut down
			return m_status.dwWin32ExitCode;
		}
		else
		{
			// Start service in the console
			ThreadPtr<WaitEsc> waitEsc { Thread::Create };
			waitEsc->Start(m_stopCtl);
		
			Console::Out("Starting service in console. Press Esc to exit\r\n\r\n");

			OnExit onExit { [&]
				{
					m_stopCtl->Stop("Service: CallRun returned without signaling stop event");
					m_stopCtl->WaitAll();
				} };
		
			CallRun(argc, argv);
			return m_status.dwWin32ExitCode;
		}
	}


	static void WINAPI CallWinServiceCtrlHandler(DWORD opCode)
	{
		Service::GetInstance().ServiceCtrlHandler(opCode);
	}


	void Service::ServiceMain(DWORD argc, LPWSTR* argv)
	{
		m_startedAsService = true;
		SetEnsureReportHandler_EventLog(ServiceName(), LogEventId());

		m_status.dwCurrentState = SERVICE_START_PENDING;
		m_serviceStatusHandle = RegisterServiceCtrlHandlerW(ServiceName(), CallWinServiceCtrlHandler);
		if (!m_serviceStatusHandle)
			LogError(Str("Error registering service control handler: ").Fun(DescribeWinErr, GetLastError()));
		else
			CallRun(NumCast<int>(argc), argv);
	}


	void Service::CallRun(int argc, wchar_t const* const* argv)
	{
		try
		{
			Str modulePath = GetModulePath();
			Seq moduleDir = GetDirectoryOfFileName(modulePath);
			WinStr moduleDirW { moduleDir };
			if (!SetCurrentDirectoryW(moduleDirW.Z()))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": SetCurrentDirectoryW"); }
		}
		catch (std::exception const& e)
		{
			LogError(Str::Join("Error changing directory to module path: ", e.what()));
			m_status.dwWin32ExitCode = ERROR_EXCEPTION_IN_SERVICE;
			return;
		}

		SetState(SERVICE_START_PENDING);

		try
		{
			m_status.dwWin32ExitCode = Run(argc, argv);
		}
		catch (std::exception const& e)
		{
			LogError(Str::Join(__FUNCTION__ ": Exception in Service::Run: ", e.what()));
			m_status.dwWin32ExitCode = ERROR_EXCEPTION_IN_SERVICE;
			return;
		}

		SetState(SERVICE_STOPPED);
	}


	void Service::ServiceCtrlHandler(DWORD opCode)
	{
		switch (opCode)
		{
		case SERVICE_CONTROL_STOP:        Stop("SERVICE_CONTROL_STOP notification received");         break;
		case SERVICE_CONTROL_PRESHUTDOWN: Stop("SERVICE_CONTROL_PRESHUTDOWN notification received");  break;
		case SERVICE_CONTROL_SHUTDOWN:    Stop("SERVICE_CONTROL_SHUTDOWN notification received");     break;
		case SERVICE_CONTROL_INTERROGATE: ::SetServiceStatus(m_serviceStatusHandle, &m_status);       break;
		default:                          LogError(Str("Unexpected service request: ").UInt(opCode));
		}
	}


	void Service::SetState(DWORD state) noexcept
	{
		m_status.dwCurrentState = state;
		if (m_startedAsService)
			SetServiceStatus(m_serviceStatusHandle, &m_status);
	}


	void Service::Stop(Seq reason)
	{
		switch (m_status.dwCurrentState)
		{
		case SERVICE_STOP_PENDING:
		case SERVICE_STOPPED:
			break;
		
		default:
			SetState(SERVICE_STOP_PENDING);
			m_stopCtl->Stop(reason);
		}
	}


	void Service::LogEvent(WORD eventType, Seq text)
	{
		text = text.Trim();

		if (!m_startedAsService)
		{
			char const* zEventType { LogEventType::Name(eventType) };
		
			SYSTEMTIME sysTime;
			GetLocalTime(&sysTime);
		
			Str full;
			full.ReserveExact(100 + text.n);
			full.Set(zEventType)
				.Add(" ").UInt(sysTime.wHour, 10, 2)
				.Add(":").UInt(sysTime.wMinute, 10, 2)
				.Add(":").UInt(sysTime.wSecond, 10, 2)
				.Add(".").UInt(sysTime.wMilliseconds, 10, 3)
				.Add(" ").Add(text).Add("\r\n\r\n");

			Console::Out(full);
		}

		HANDLE eventSource = RegisterEventSourceW(0, ServiceName());
		if (eventSource)
		{
			WinStr textW(text);
			LPCWSTR textPtr = textW.Z();
			ReportEventW(eventSource, eventType, 0, LogEventId(), 0, 1, 0, &textPtr, 0);
			DeregisterEventSource(eventSource);
		}
	}


	void Service::WaitServiceStop(Seq initInfo)
	{
		SetState(SERVICE_RUNNING);
		LogInfo(Str("Service running. ").Add(initInfo));

		Wait1(m_stopCtl->StopEvent().Handle(), INFINITE);
		LogInfo(Str("Service stopping: ").Add(m_stopCtl->StopReason()));

		m_stopCtl->WaitAll();
	}

}
