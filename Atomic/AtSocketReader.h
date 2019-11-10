#pragma once

#include "AtOvlReader.h"
#include "AtSocket.h"


namespace At
{
	class SocketReader : virtual public OvlReader
	{
	public:
		SocketReader(SOCKET s = INVALID_SOCKET) : m_s(s) {}

		void SetSocket(SOCKET s) { EnsureThrow(m_s == INVALID_SOCKET); m_s = s; }

	protected:
		IoResult::E OvlReader_Read(void* buf, DWORD nrBytesToRead, DWORD& nrBytesRead, OVERLAPPED& ovl, int64& errorCode) override final;
		IoResult::E OvlReader_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesRead, bool wait, int64& errorCode) override final;
		void OvlReader_Cancel(OVERLAPPED& ovl) override final;

		SOCKET m_s { INVALID_SOCKET };
		WSABUF m_wb;

		struct State { enum E { Ready, Pending, Ended }; };
		State::E m_state { State::Ready };
	};
}
