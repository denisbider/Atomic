#include "AtIncludes.h"
#include "AtEvent.h"


namespace At
{

	void Event::Close()
	{
		if (m_h != 0)
		{
			EnsureReportWithNr(CloseHandle(m_h), GetLastError());
			m_h = 0;
		}
	}


	void Event::Create(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initState, LPCTSTR name)
	{
		EnsureThrow(m_h == 0);
		m_h = CreateEventW(sa, manual, initState, name);
		if (!m_h)
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": Error in CreateEventW"); }
	}


	void Event::Signal(HANDLE h)
	{
		if (!SetEvent(h) && !std::uncaught_exception())
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": Error in SetEvent"); }
	}


	void Event::ClearSignal(HANDLE h)
	{
		if (!ResetEvent(h) && !std::uncaught_exception())
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": Error in ResetEvent"); }
	}


	bool Event::Wait(HANDLE h, DWORD waitMs)
	{
		DWORD waitResult = WaitForSingleObject(h, waitMs);
		if (waitResult == 0)	// WAIT_OBJECT_0
			return true;
		if (waitResult == WAIT_FAILED)
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": WaitForSingleObject failed"); }
		if (waitResult != WAIT_TIMEOUT)
			throw ZLitErr(__FUNCTION__ ": Unexpected wait result");
		return false;
	}


	void Event::WaitIndefinite(HANDLE h)
	{
		bool waitSuccessful = Wait(h, INFINITE);
		EnsureThrow(waitSuccessful);
	}

}
