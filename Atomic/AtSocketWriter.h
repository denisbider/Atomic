#pragma once

#include "AtOvlWriter.h"
#include "AtSocket.h"


namespace At
{
	class SocketWriter : virtual public OvlWriter
	{
	public:
		SocketWriter(SOCKET s = INVALID_SOCKET) : m_s(s) {}

		void SetSocket(SOCKET s) { EnsureThrow(m_s == INVALID_SOCKET); m_s = s; }

		void Shutdown() override final;

	protected:
		IoResult::E OvlWriter_Write(void const* buf, DWORD nrBytesToWrite, DWORD& nrBytesWritten, OVERLAPPED& ovl, int64& errorCode) override final;
		IoResult::E OvlWriter_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesWritten, bool wait, int64& errorCode) override final;
		void OvlWriter_Cancel(OVERLAPPED& ovl) override final;

		SOCKET m_s { INVALID_SOCKET };
		WSABUF m_wb;

		struct State { enum E { Ready, Pending, Ended }; };
		State::E m_state { State::Ready };
	};
}
