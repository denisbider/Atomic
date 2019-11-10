#include "AtIncludes.h"
#include "AtSocketWriter.h"

#include "AtDllKernel32.h"


namespace At
{
	OvlWriter::IoResult::E SocketWriter::OvlWriter_Write(void const* buf, DWORD nrBytesToWrite, DWORD& nrBytesWritten, OVERLAPPED& ovl, int64& errorCode)
	{
		EnsureThrow(m_s != INVALID_SOCKET);
		EnsureThrow(m_state == State::Ready);

		m_wb.buf = (CHAR*) buf;
		m_wb.len = nrBytesToWrite;

		if (WSASend(m_s, &m_wb, 1, &nrBytesToWrite, 0, &ovl, 0) == 0)
		{
			nrBytesWritten = nrBytesToWrite;
			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = (DWORD) WSAGetLastError();
		if (errorCode == WSA_IO_PENDING)
		{
			m_state = State::Pending;
			return IoResult::Pending;
		}

		m_state = State::Ended;
		return IoResult::Error;
	}


	OvlWriter::IoResult::E SocketWriter::OvlWriter_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesWritten, bool wait, int64& errorCode)
	{
		EnsureThrow(m_state == State::Pending);

		DWORD flags = 0;
		if (WSAGetOverlappedResult(m_s, &ovl, &nrBytesWritten, wait, &flags))
		{
			m_state = State::Ready;
			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = (DWORD) WSAGetLastError();
		if (errorCode == WSA_IO_PENDING)
			return IoResult::Pending;

		m_state = State::Ended;
		return IoResult::Error;
	}


	void SocketWriter::OvlWriter_Cancel(OVERLAPPED& ovl)
	{
		if (m_state == State::Pending)
		{
			m_state = State::Ended;

			CancelIoEx_or_CancelIo((HANDLE) m_s, &ovl);

			DWORD nrBytesWritten = 0;
			DWORD flags = 0;
			WSAGetOverlappedResult(m_s, &ovl, &nrBytesWritten, TRUE, &flags);
		}
	}

	void SocketWriter::Shutdown()
	{
		EnsureThrow(m_s != INVALID_SOCKET);
		EnsureThrow(m_state == State::Ready);

		m_state = State::Ended;

		if (WSAEventSelect(m_s, m_writeEvent.Handle(), FD_CLOSE) != 0)
			throw WriteWinErr(WSAGetLastError());

		if (shutdown(m_s, SD_SEND) != 0)
			throw WriteWinErr(WSAGetLastError());

		AbortableWait(m_writeEvent.Handle());
	}
}
