#include "AtIncludes.h"
#include "AtEnsure.h"

#include "AtEnsureConvert.h"
#include "AtEnsureFailDesc.h"
#include "AtEnsureStackTrace.h"
#include "AtToolhelp.h"


namespace At
{

	namespace Internal
	{

		typedef bool (*EnsureReportHandler)(char const* desc);

		EnsureFailHandler   g_ensureFailHandler                       {};
		EnsureReportHandler g_ensureReportHandler                     { EnsureReportHandler_Interactive };
		wchar_t const*      g_ensureReportHandler_eventLog_sourceName {};
		DWORD               g_ensureReportHandler_eventLog_eventId    {};


		struct EnsureFailureException : public InternalInconsistency
		{
			EnsureFailureException(EnsureFailDescRef&& efdRef) noexcept : m_efdRef(std::move(efdRef)) {}
			char const* what() const override { return m_efdRef.Z(); }
		private:
			EnsureFailDescRef m_efdRef;
		};

	}	// Internal

	using namespace Internal;


	bool EnsureReportHandler_Interactive(char const* desc)
	{
		bool writtenToStdErr {};

		HANDLE hOut { GetStdHandle(STD_ERROR_HANDLE) };
		if (hOut != INVALID_HANDLE_VALUE)
		{
			UINT cp = GetConsoleOutputCP();
			EnsureConvert_AtoW descW { desc, CP_UTF8 };
			EnsureConvert_WtoA descA { descW.Z(), cp };
			DWORD bytesWritten {};
			if (WriteFile(hOut, descA.Z(), (DWORD) strlen(descA.Z()), &bytesWritten, 0))
				writtenToStdErr = true;
		}

		if (!writtenToStdErr)
		{
			EnsureConvert_AtoW descW { desc, CP_UTF8 };
			MessageBoxW(0, descW.Z(), L"Ensure failure", MB_ICONERROR | MB_OK | MB_TASKMODAL);
		}

		return true;
	}


	bool EnsureReportHandler_EventLog(char const* desc)
	{
		bool success {};
		HANDLE eventSource { RegisterEventSourceW(0, g_ensureReportHandler_eventLog_sourceName) };
		if (eventSource)
		{
			EnsureConvert_AtoW descW { desc, CP_UTF8 };
			LPCWSTR textPtr { descW.Z() };
			if (ReportEventW(eventSource, EVENTLOG_ERROR_TYPE, 0, g_ensureReportHandler_eventLog_eventId, 0, 1, 0, &textPtr, 0))
				success = true;
			DeregisterEventSource(eventSource);
		}
		return success;
	}

	
	void SetEnsureFailHandler(EnsureFailHandler handler)
	{
		g_ensureFailHandler = handler;
	}


	void SetEnsureReportHandler_EventLog(wchar_t const* sourceName, DWORD eventId)
	{
		g_ensureReportHandler                     = EnsureReportHandler_EventLog;
		g_ensureReportHandler_eventLog_sourceName = sourceName;
		g_ensureReportHandler_eventLog_eventId    = eventId;
	}


	void EnsureFail(OnFail::E onFail, EnsureFailDescRef&& efdRef, ULONG extraFramesToSkip)
	{
		if (g_ensureFailHandler)
		{
			// The application is using a custom EnsureFail handler
			g_ensureFailHandler(onFail, efdRef.Z());
			return;
		}

		if (onFail == OnFail::Throw && std::uncaught_exception())
			onFail = OnFail::Abort;

		if (onFail == OnFail::Abort)
			SuspendAllOtherThreads();

		if (IsDebuggerPresent())
			DebugBreak();
		
		// Add to error description
		Ensure_AddStackTrace(efdRef, 2U + extraFramesToSkip, 15U);

	#ifdef _DEBUG
		bool waitForDebugger = (onFail != OnFail::Throw) && !IsDebuggerPresent();
		if (waitForDebugger)
			efdRef.Add("\r\nPlease attach a debugger or use other means to terminate the process\r\n");
	#endif
		
		if (onFail == OnFail::Throw)
			throw EnsureFailureException(std::move(efdRef));

		bool reportSuccessful {};
		if (g_ensureReportHandler != nullptr)
			reportSuccessful = g_ensureReportHandler(efdRef.Z());

	#ifdef _DEBUG
		if (waitForDebugger)
		{
			while (!IsDebuggerPresent()) Sleep(100);
			DebugBreak();
		}
	#endif

		if (onFail != OnFail::Report || !reportSuccessful)
			ExitProcess(0x52534E45);	// ASCII "ENSR" (little-endian), decimal 1381191237
	}

	struct EnsureFailDescBasic
	{
		EnsureFailDescBasic(char const* locAndDesc) : m_efdRef(EnsureFailDesc::Create())
			{ m_efdRef.Add("Ensure check failed at ").Add(locAndDesc).Add("\r\n"); }

		EnsureFailDescRef m_efdRef;
	};

	__declspec(noinline) void EnsureFail_Report (char const* locAndDesc) { EnsureFailDescBasic d { locAndDesc }; EnsureFail(OnFail::Report, std::move(d.m_efdRef), 0); }
	__declspec(noinline) void EnsureFail_Throw  (char const* locAndDesc) { EnsureFailDescBasic d { locAndDesc }; EnsureFail(OnFail::Throw,  std::move(d.m_efdRef), 0); }
	__declspec(noinline) void EnsureFail_Abort  (char const* locAndDesc) { EnsureFailDescBasic d { locAndDesc }; EnsureFail(OnFail::Abort,  std::move(d.m_efdRef), 0); }


	__declspec(noinline) void EnsureFailWithNr(OnFail::E onFail, char const* locAndDesc, int64 nr)
	{
		EnsureFailDescBasic d { locAndDesc };
		d.m_efdRef.Add("Value: ").SInt(nr).Add("\r\n");
		EnsureFail(onFail, std::move(d.m_efdRef), 1);
	}

	__declspec(noinline) void EnsureFailWithNr_Report (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Report, locAndDesc, nr); }
	__declspec(noinline) void EnsureFailWithNr_Throw  (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Throw,  locAndDesc, nr); }
	__declspec(noinline) void EnsureFailWithNr_Abort  (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Abort,  locAndDesc, nr); }


	__declspec(noinline) void EnsureFailWithNr2(OnFail::E onFail, char const* locAndDesc, int64 n1, int64 n2)
	{
		EnsureFailDescBasic d { locAndDesc };
		d.m_efdRef.Add("Value 1: ").SInt(n1).Add("\r\nValue 2: ").SInt(n2).Add("\r\n");
		EnsureFail(onFail, std::move(d.m_efdRef), 1);
	}

	__declspec(noinline) void EnsureFailWithNr2_Report (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Report, locAndDesc, n1, n2); }
	__declspec(noinline) void EnsureFailWithNr2_Throw  (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Throw,  locAndDesc, n1, n2); }
	__declspec(noinline) void EnsureFailWithNr2_Abort  (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Abort,  locAndDesc, n1, n2); }


	__declspec(noinline) void EnsureFailWithDesc(OnFail::E onFail, char const* desc, char const* funcOrFile, long line)
	{
		EnsureFailDescRef efdRef { EnsureFailDesc::Create() };
		efdRef.Add("Ensure check failed at \"").Add(funcOrFile).Add("\", line ").SInt(line).Add(":\r\n").Add(desc).Add("\r\n");
		EnsureFail(onFail, std::move(efdRef), 0);
	}

}
