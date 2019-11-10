#pragma once

#include "AtOvlWriter.h"


namespace At
{

	class HandleWriter : virtual public OvlWriter
	{
	public:
		HandleWriter() = default;
		HandleWriter(HANDLE h) : m_h(h) {}

		void SetHandle(HANDLE h) { EnsureThrow(m_h == INVALID_HANDLE_VALUE); m_h = h; m_offset = 0; m_setOffset = (GetFileType(h) == FILE_TYPE_DISK); }
		void SetOffset(uint64 o) { EnsureThrow(m_h != INVALID_HANDLE_VALUE); m_offset = o; }

		void Shutdown() override final {}

	protected:
		IoResult::E OvlWriter_Write(void const* buf, DWORD nrBytesToWrite, DWORD& nrBytesWritten, OVERLAPPED& ovl, int64& errorCode) override final;
		IoResult::E OvlWriter_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesWritten, bool wait, int64& errorCode) override final;
		void OvlWriter_Cancel(OVERLAPPED& ovl) override final;

		HANDLE m_h         { INVALID_HANDLE_VALUE };
		uint64 m_offset    {};
		bool   m_setOffset {};

		struct State { enum E { Ready, Pending, Ended }; };
		State::E m_state { State::Ready };
	};

}
