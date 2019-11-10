#include "AtIncludes.h"
#include "AtHandleReader.h"

#include "AtDllKernel32.h"


namespace At
{

	OvlReader::IoResult::E HandleReader::OvlReader_Read(void* buf, DWORD nrBytesToRead, DWORD& nrBytesRead, OVERLAPPED& ovl, int64& errorCode)
	{
		EnsureThrow(m_h != INVALID_HANDLE_VALUE);
		EnsureThrow(m_state == State::Ready);

		if (m_setOffset)
		{
			ovl.OffsetHigh = (DWORD) ((m_offset >> 32) & 0xFFFFFFFFU);
			ovl.Offset     = (DWORD) ( m_offset        & 0xFFFFFFFFU);
		}

		if (ReadFile(m_h, buf, nrBytesToRead, &nrBytesRead, &ovl))
		{
			m_offset += nrBytesRead;
			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = GetLastError();
		if (errorCode == ERROR_IO_PENDING)
		{
			m_state = State::Pending;
			return IoResult::Pending;
		}

		m_state = State::Ended;
		if (errorCode == ERROR_HANDLE_EOF)
			return IoResult::ReachedEnd;
		return IoResult::Error;
	}


	OvlReader::IoResult::E HandleReader::OvlReader_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesRead, bool wait, int64& errorCode)
	{
		EnsureThrow(m_state == State::Pending);

		if (GetOverlappedResult(m_h, &ovl, &nrBytesRead, wait))
		{
			m_offset += nrBytesRead;
			m_state = State::Ready;
			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = GetLastError();
		if (errorCode == ERROR_IO_PENDING)
			return IoResult::Pending;

		m_state = State::Ended;
		if (errorCode == ERROR_HANDLE_EOF)
			return IoResult::ReachedEnd;
		return IoResult::Error;
	}


	void HandleReader::OvlReader_Cancel(OVERLAPPED& ovl)
	{
		if (m_state == State::Pending)
		{
			m_state = State::Ended;

			CancelIoEx_or_CancelIo(m_h, &ovl);

			DWORD nrBytesRead = 0;
			GetOverlappedResult(m_h, &ovl, &nrBytesRead, TRUE);
		}
	}

}
