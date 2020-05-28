#include "AtIncludes.h"
#include "AtEnsure.h"

#include "AtToolhelp.h"


namespace At
{

	namespace
	{
		enum { MaxDescChars = 300 };

		typedef bool (*EnsureReportHandler)(char const* desc);

		bool EnsureReportHandler_Interactive(char const* desc);
		bool EnsureReportHandler_EventLog   (char const* desc);


		EnsureFailHandler   g_ensureFailHandler                       {};
		EnsureReportHandler g_ensureReportHandler                     { EnsureReportHandler_Interactive };
		wchar_t const*      g_ensureReportHandler_eventLog_sourceName {};
		DWORD               g_ensureReportHandler_eventLog_eventId    {};


		struct EnsureFailureException : public InternalInconsistency
		{
			EnsureFailureException(char const* desc) { strcpy_s<MaxDescChars>(m_desc, desc); }
			char const* what() const override { return m_desc; }
		private:
			char m_desc[MaxDescChars];
		};

		
		struct EnsureConvert_WtoA
		{
			EnsureConvert_WtoA(wchar_t const* descW, UINT cp)
			{
				int len { WideCharToMultiByte(cp, 0, descW, -1, m_desc, MaxDescChars, 0, 0) };
				if (len <= 0)
					strcpy_s<MaxDescChars>(m_desc, "Ensure check failed. Additionally, an error occurred converting the failure description from UCS-2.");
			}

			char m_desc[MaxDescChars];
		};

		
		struct EnsureConvert_AtoW
		{
			EnsureConvert_AtoW(char const* descA)
			{
				int len { MultiByteToWideChar(CP_UTF8, 0, descA, -1, m_desc, MaxDescChars) };
				if (len <= 0)
					wcscpy_s<MaxDescChars>(m_desc, L"Ensure check failed. Additionally, an error occurred converting the failure description to UCS-2.");
			}

			wchar_t m_desc[MaxDescChars];
		};


		bool EnsureReportHandler_Interactive(char const* desc)
		{
			bool writtenToStdErr {};

			HANDLE hOut { GetStdHandle(STD_ERROR_HANDLE) };
			if (hOut != INVALID_HANDLE_VALUE)
			{
				UINT cp = GetConsoleOutputCP();
				EnsureConvert_AtoW descW { desc };
				EnsureConvert_WtoA descA { descW.m_desc, cp };
				DWORD bytesWritten {};
				if (WriteFile(hOut, descA.m_desc, (DWORD) strlen(descA.m_desc), &bytesWritten, 0))
					writtenToStdErr = true;
			}

			if (!writtenToStdErr)
			{
				EnsureConvert_AtoW descW(desc);
				MessageBoxW(0, descW.m_desc, L"Ensure failure", MB_ICONERROR | MB_OK | MB_TASKMODAL);
			}

			return true;
		}


		bool EnsureReportHandler_EventLog(char const* desc)
		{
			bool success {};
			HANDLE eventSource { RegisterEventSourceW(0, g_ensureReportHandler_eventLog_sourceName) };
			if (eventSource)
			{
				EnsureConvert_AtoW descW { desc };
				LPCWSTR textPtr { descW.m_desc };
				if (ReportEventW(eventSource, EVENTLOG_ERROR_TYPE, 0, g_ensureReportHandler_eventLog_eventId, 0, 1, 0, &textPtr, 0))
					success = true;
				DeregisterEventSource(eventSource);
			}
			return success;
		}
	} // anonymous namespace

	
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


	void EnsureFail(OnFail::E onFail, char const* locAndDesc)
	{
		if (g_ensureFailHandler)
		{
			// The application is using a custom EnsureFail handler
			g_ensureFailHandler(onFail, locAndDesc);
			return;
		}

		if (onFail == OnFail::Throw && std::uncaught_exception())
			onFail = OnFail::Abort;

		if (onFail == OnFail::Abort)
			SuspendAllOtherThreads();

		if (IsDebuggerPresent())
			DebugBreak();
		
		// Prepare error description
		char desc[MaxDescChars];
		StringCchPrintfA(desc, MaxDescChars, "Ensure check failed at %s\r\n", locAndDesc);
		
		if (onFail == OnFail::Throw)
			throw EnsureFailureException(desc);

		bool reportSuccessful {};
		if (g_ensureReportHandler != nullptr)
			reportSuccessful = g_ensureReportHandler(desc);

	#ifdef _DEBUG
		if (!IsDebuggerPresent())
		{
			do Sleep(100); while (!IsDebuggerPresent());
			DebugBreak();
		}
	#endif

		if (onFail != OnFail::Report || !reportSuccessful)
			ExitProcess(0x52534E45);	// ASCII "ENSR" (little-endian), decimal 1381191237
	}

	__declspec(noinline) void EnsureFail_Report (char const* locAndDesc) { EnsureFail(OnFail::Report, locAndDesc); }
	__declspec(noinline) void EnsureFail_Throw  (char const* locAndDesc) { EnsureFail(OnFail::Throw,  locAndDesc); }
	__declspec(noinline) void EnsureFail_Abort  (char const* locAndDesc) { EnsureFail(OnFail::Abort,  locAndDesc); }


	__declspec(noinline) void EnsureFailWithNr(OnFail::E onFail, char const* locAndDesc, int64 nr)
	{
		char locDescAndNr[MaxDescChars];
		StringCchPrintfA(locDescAndNr, MaxDescChars, "%s\r\nValue: %I64i", locAndDesc, nr);
		EnsureFail(onFail, locDescAndNr);
	}

	__declspec(noinline) void EnsureFailWithNr_Report (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Report, locAndDesc, nr); }
	__declspec(noinline) void EnsureFailWithNr_Throw  (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Throw,  locAndDesc, nr); }
	__declspec(noinline) void EnsureFailWithNr_Abort  (char const* locAndDesc, int64 nr) { EnsureFailWithNr(OnFail::Abort,  locAndDesc, nr); }


	__declspec(noinline) void EnsureFailWithNr2(OnFail::E onFail, char const* locAndDesc, int64 n1, int64 n2)
	{
		char locDescAndNrs[MaxDescChars];
		StringCchPrintfA(locDescAndNrs, MaxDescChars, "%s\r\nValue 1: %I64i\r\nValue 2: %I64i", locAndDesc, n1, n2);
		EnsureFail(onFail, locDescAndNrs);
	}

	__declspec(noinline) void EnsureFailWithNr2_Report (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Report, locAndDesc, n1, n2); }
	__declspec(noinline) void EnsureFailWithNr2_Throw  (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Throw,  locAndDesc, n1, n2); }
	__declspec(noinline) void EnsureFailWithNr2_Abort  (char const* locAndDesc, int64 n1, int64 n2) { EnsureFailWithNr2(OnFail::Abort,  locAndDesc, n1, n2); }


	__declspec(noinline) void EnsureFailWithDesc(OnFail::E onFail, char const* desc, char const* funcOrFile, long line)
	{
		char locAndDesc[MaxDescChars];
		StringCchPrintfA(locAndDesc, MaxDescChars, "\"%s\", line %li:\r\n%s", funcOrFile, line, desc);
		EnsureFail(onFail, locAndDesc);
	}

}
