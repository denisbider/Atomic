#pragma once

#include "AtIncludes.h"
#include "AtNum.h"


namespace At
{

	// By default, an interactive EnsureReportHandler is set. The interactive handler will output information
	// about an Ensure failure either to STDOUT (if available), or otherwise, using an interactive dialog box.
	//
	// If the application is running non-interactively, call this to set an EnsureReportHandler that will
	// output information about any ensure failures to the Application section of the Windows Event Log.
	//
	// The event text associated with the provided ID must accept a single string parameter, which receives
	// a description of the Ensure failure.

	void SetEnsureReportHandler_EventLog(wchar_t const* sourceName, DWORD eventId);


	struct OnFail { enum E { Report, Throw, Abort }; };

	struct InternalInconsistency : public std::exception {};

	// Handles an Ensure failure:
	// - if running under a debugger, always causes a debug breakpoint before anything else;
	// - if onFail == OnFail::Throw, and there is no uncaught exception, throws an exception deriving from InternalInconsistency;
	// - if onFail == OnFail::Abort, or Throw and there is an uncaught exception, calls the registered EnsureReportHandler and exits;
	// - if onFail == OnFail::Report, calls the registered EnsureReportHandler and continues if the handler succeeded; otherwise, exits.
	
	__declspec(noinline) void EnsureFail(OnFail::E onFail, char const* locAndDesc);

	__declspec(noinline) void EnsureFail_Report (char const* locAndDesc);
	__declspec(noinline) void EnsureFail_Throw  (char const* locAndDesc);
	__declspec(noinline) void EnsureFail_Abort  (char const* locAndDesc);

	#define EnsureReport(TEST)   ((TEST) ? 0 : (At::EnsureFail_Report ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))
	#define EnsureThrow(TEST)    ((TEST) ? 0 : (At::EnsureFail_Throw  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))
	#define EnsureAbort(TEST)    ((TEST) ? 0 : (At::EnsureFail_Abort  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST), 0))


	__declspec(noinline) void EnsureFailWithCode(OnFail::E onFail, char const* locAndDesc, int64 code);

	__declspec(noinline) void EnsureFailWithCode_Report (char const* locAndDesc, int64 code);
	__declspec(noinline) void EnsureFailWithCode_Throw  (char const* locAndDesc, int64 code);
	__declspec(noinline) void EnsureFailWithCode_Abort  (char const* locAndDesc, int64 code);

	#define EnsureReportWithCode(TEST, CODE)   ((TEST) ? 0 : (At::EnsureFailWithCode_Report ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, CODE), 0))
	#define EnsureThrowWithCode(TEST, CODE)    ((TEST) ? 0 : (At::EnsureFailWithCode_Throw  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, CODE), 0))
	#define EnsureAbortWithCode(TEST, CODE)    ((TEST) ? 0 : (At::EnsureFailWithCode_Abort  ("\"" __FUNCTION__ "\", line " AT_EXPAND_STRINGIFY(__LINE__) ":\r\n" #TEST, CODE), 0))


	__declspec(noinline) void EnsureFailWithDesc(OnFail::E onFail, char const* desc, char const* funcOrFile, long line);

}
