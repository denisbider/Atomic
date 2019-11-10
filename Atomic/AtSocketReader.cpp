#include "AtIncludes.h"
#include "AtSocketReader.h"

#include "AtDllKernel32.h"


namespace At
{
	OvlReader::IoResult::E SocketReader::OvlReader_Read(void* buf, DWORD nrBytesToRead, DWORD& nrBytesRead, OVERLAPPED& ovl, int64& errorCode)
	{
		EnsureThrow(m_s != INVALID_SOCKET);
		EnsureThrow(m_state == State::Ready);

		m_wb.buf = (CHAR*) buf;
		m_wb.len = nrBytesToRead;

		DWORD flags = 0;
		if (WSARecv(m_s, &m_wb, 1, &nrBytesRead, &flags, &ovl, 0) == 0)
		{
			if (nrBytesRead == 0)
				return IoResult::ReachedEnd;

			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = WSAGetLastError();
		if (errorCode == WSA_IO_PENDING)
		{
			m_state = State::Pending;
			return IoResult::Pending;
		}

		m_state = State::Ended;
		return IoResult::Error;
	}


	OvlReader::IoResult::E SocketReader::OvlReader_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesRead, bool wait, int64& errorCode)
	{
		EnsureThrow(m_state == State::Pending);

		DWORD flags = 0;
		if (WSAGetOverlappedResult(m_s, &ovl, &nrBytesRead, wait, &flags))
		{
			m_state = State::Ready;

			if (nrBytesRead == 0)
				return IoResult::ReachedEnd;

			errorCode = 0;
			return IoResult::Done;
		}

		errorCode = WSAGetLastError();
		if (errorCode == WSA_IO_PENDING)
			return IoResult::Pending;

		m_state = State::Ended;
		return IoResult::Error;
	}


	void SocketReader::OvlReader_Cancel(OVERLAPPED& ovl)
	{
		if (m_state == State::Pending)
		{
			m_state = State::Ended;

			CancelIoEx_or_CancelIo((HANDLE) m_s, &ovl);

			DWORD nrBytesRead = 0;
			DWORD flags = 0;
			WSAGetOverlappedResult(m_s, &ovl, &nrBytesRead, TRUE, &flags);
		}
	}
}
