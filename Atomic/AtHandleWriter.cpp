#include "AtIncludes.h"
#include "AtHandleWriter.h"

#include "AtDllKernel32.h"


namespace At
{

	OvlWriter::IoResult::E HandleWriter::OvlWriter_Write(void const* buf, DWORD nrBytesToWrite, DWORD& nrBytesWritten, OVERLAPPED& ovl, int64& errorCode)
	{
		EnsureThrow(m_h != INVALID_HANDLE_VALUE);
		EnsureThrow(m_state == State::Ready);

		if (m_setOffset)
		{
			ovl.OffsetHigh = (DWORD) ((m_offset >> 32) & 0xFFFFFFFFU);
			ovl.Offset     = (DWORD) ( m_offset        & 0xFFFFFFFFU);
		}

		if (WriteFile(m_h, buf, nrBytesToWrite, &nrBytesWritten, &ovl))
		{
			m_offset += nrBytesWritten;
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
		return IoResult::Error;
	}


	OvlWriter::IoResult::E HandleWriter::OvlWriter_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesWritten, bool wait, int64& errorCode)
	{
		EnsureThrow(m_state == State::Pending);

		if (GetOverlappedResult(m_h, &ovl, &nrBytesWritten, wait))
		{
			m_offset += nrBytesWritten;
			m_state = State::Ready;
			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = GetLastError();
		if (errorCode == ERROR_IO_PENDING)
			return IoResult::Pending;

		m_state = State::Ended;
		return IoResult::Error;
	}


	void HandleWriter::OvlWriter_Cancel(OVERLAPPED& ovl)
	{
		if (m_state == State::Pending)
		{
			m_state = State::Ended;

			CancelIoEx_or_CancelIo(m_h, &ovl);

			DWORD nrBytesWritten = 0;
			GetOverlappedResult(m_h, &ovl, &nrBytesWritten, TRUE);
		}
	}

}
