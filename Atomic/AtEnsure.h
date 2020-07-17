#pragma once

#include "AtIncludes.h"
#include "AtNum.h"


namespace At
{

	struct OnFail { enum E { Report, Throw, Abort }; };


	// This completely overrides all Ensure failure handling normally provided by Atomic.
	// Use this if your application has your own Ensure failure handlers with desirable functionality.

	typedef void (*EnsureFailHandler)(OnFail::E onFail, char const* locAndDesc);

	void SetEnsureFailHandler(EnsureFailHandler handler);


	// By default, an interactive EnsureReportHandler is set. The interactive handler will output information
	// about an Ensure failure either to STDOUT (if available), or otherwise, using an interactive dialog box.
	//
	// If the application is running non-interactively, call this to set an EnsureReportHandler that will
	// output information about any ensure failures to the Application section of the Windows Event Log.
	//
	// The event text associated with the provided ID must accept a single string parameter, which receives
	// a description of the Ensure failure.

	void SetEnsureReportHandler_EventLog(wchar_t const* sourceName, DWORD eventId);

	bool EnsureReportHandler_Interactive (char const* desc);
	bool EnsureReportHandler_EventLog    (char const* desc);


	struct InternalInconsistency : public std::exception {};

	// Handles an Ensure failure:
	// - if running under a debugger, always causes a debug breakpoint before anything else;
	// - if onFail == OnFail::Throw, and there is no uncaught exception, throws an exception deriving from InternalInconsistency;
	// - if onFail == OnFail::Abort, or Throw and there is an uncaught exception, calls the registered EnsureReportHandler and exits;
	// - if onFail == OnFail::Report, calls the registered EnsureReportHandler and continues if the handler succeeded; otherwise, exits.

	__declspec(noinline) void EnsureFail_Report (char const* locAndDesc);
	__declspec(noinline) void EnsureFail_Throw  (char const* locAndDesc);
	__declspec(noinline) void EnsureFail_Abort  (char const* locAndDesc);

	#define EnsureReport(TEST)   ((TEST) ? 0 : (At::EnsureFail_Report ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))
	#define EnsureThrow(TEST)    ((TEST) ? 0 : (At::EnsureFail_Throw  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))
	#define EnsureAbort(TEST)    ((TEST) ? 0 : (At::EnsureFail_Abort  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))


	__declspec(noinline) void EnsureFailWithNr(OnFail::E onFail, char const* locAndDesc, int64 nr);

	__declspec(noinline) void EnsureFailWithNr_Report (char const* locAndDesc, int64 nr);
	__declspec(noinline) void EnsureFailWithNr_Throw  (char const* locAndDesc, int64 nr);
	__declspec(noinline) void EnsureFailWithNr_Abort  (char const* locAndDesc, int64 nr);

	#define EnsureReportWithNr(TEST, NR)   ((TEST) ? 0 : (At::EnsureFailWithNr_Report ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, NR), 0))
	#define EnsureThrowWithNr(TEST, NR)    ((TEST) ? 0 : (At::EnsureFailWithNr_Throw  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, NR), 0))
	#define EnsureAbortWithNr(TEST, NR)    ((TEST) ? 0 : (At::EnsureFailWithNr_Abort  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, NR), 0))


	__declspec(noinline) void EnsureFailWithNr2(OnFail::E onFail, char const* locAndDesc, int64 n1, int64 n2);

	__declspec(noinline) void EnsureFailWithNr2_Report (char const* locAndDesc, int64 n1, int64 n2);
	__declspec(noinline) void EnsureFailWithNr2_Throw  (char const* locAndDesc, int64 n1, int64 n2);
	__declspec(noinline) void EnsureFailWithNr2_Abort  (char const* locAndDesc, int64 n1, int64 n2);

	#define EnsureReportWithNr2(TEST, N1, N2)   ((TEST) ? 0 : (At::EnsureFailWithNr2_Report ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, N1, N2), 0))
	#define EnsureThrowWithNr2(TEST, N1, N2)    ((TEST) ? 0 : (At::EnsureFailWithNr2_Throw  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, N1, N2), 0))
	#define EnsureAbortWithNr2(TEST, N1, N2)    ((TEST) ? 0 : (At::EnsureFailWithNr2_Abort  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, N1, N2), 0))


	__declspec(noinline) void EnsureFailWithDesc(OnFail::E onFail, char const* desc, char const* funcOrFile, long line);

}
