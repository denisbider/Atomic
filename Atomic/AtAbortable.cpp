#include "AtIncludes.h"
#include "AtAbortable.h"

#include "AtDllKernel32.h"



namespace At
{

	void Timeouts::SetOverallExpireMs(uint64 ms)
	{
		m_overallTimeout = true;
		m_expireTickCount = SatAdd<uint64>(E_GetTickCount64(), ms);
	}


	void IAbortable::SetExpireMs(uint64 ms)
	{
		SetExpireTickCount(SatAdd<uint64>(E_GetTickCount64(), ms));
	}


	void IAbortable::ApplyTimeouts(Timeouts const& timeouts, uint64 individualMs)
	{
		if (timeouts.m_overallTimeout)
			SetExpireTickCount(timeouts.m_expireTickCount);
		else
			SetExpireMs(individualMs);
	}


	void Abortable::AbortableSleep(DWORD waitMs)
	{
		if (m_expireTickCount != UINT64_MAX)
		{
			DWORD altWaitMs = NumCast<DWORD>(SatSub<uint64>(m_expireTickCount, E_GetTickCount64()));
			if (waitMs > altWaitMs)
				waitMs = altWaitMs;
		}

		if (!m_stopCtl.Any())
			Sleep(waitMs);
		else
		{
			DWORD waitResult = Wait1(m_stopCtl->StopEvent().Handle(), waitMs);
			if (waitResult == 0)				// WAIT_OBJECT_0
				throw ExecutionAborted();
		}
	}


	void Abortable::AbortableWait(HANDLE hEvent)
	{
		DWORD waitMs = INFINITE;
		if (m_expireTickCount != UINT64_MAX)
			waitMs = NumCast<DWORD>(SatSub<uint64>(m_expireTickCount, E_GetTickCount64()));

		if (!m_stopCtl.Any())
		{
			if (Wait1(hEvent, waitMs) == WAIT_TIMEOUT)
				throw TimeExpired();
		}
		else
		{
			DWORD waitResult = Wait2(m_stopCtl->StopEvent().Handle(), hEvent, waitMs);
			if (waitResult == WAIT_TIMEOUT) throw TimeExpired();
			if (waitResult == 0)	        throw ExecutionAborted();			// WAIT_OBJECT_0
		}
	}

}
