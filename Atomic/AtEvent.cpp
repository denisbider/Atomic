#include "AtIncludes.h"
#include "AtEvent.h"


namespace At
{

	void Event::Close()
	{
		if (m_h != 0)
		{
			EnsureReportWithCode(CloseHandle(m_h), GetLastError());
			m_h = 0;
		}
	}


	void Event::Create(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initState, LPCTSTR name)
	{
		EnsureThrow(m_h == 0);
		m_h = CreateEventW(sa, manual, initState, name);
		if (!m_h)
			{ LastWinErr e; throw e.Make<>("Error in CreateEventW"); }
	}


	void Event::Signal(HANDLE h)
	{
		if (!SetEvent(h))
			{ LastWinErr e; throw e.Make<>("Error in SetEvent"); }
	}


	void Event::ClearSignal(HANDLE h)
	{
		if (!ResetEvent(h))
			{ LastWinErr e; throw e.Make<>("Error in ResetEvent"); }
	}


	bool Event::IsSignaled(HANDLE h)
	{
		DWORD waitResult { WaitForSingleObject(h, 0) };
		if (waitResult == 0)	// WAIT_OBJECT_0
			return true;
		if (waitResult == WAIT_FAILED)
			{ LastWinErr e; throw e.Make<>("AtEvent IsSignaled: WaitForSingleObject failed"); }
		if (waitResult != WAIT_TIMEOUT)
			throw ZLitErr("AtEvent IsSignaled: Unexpected wait result");
		return false;
	}

}
