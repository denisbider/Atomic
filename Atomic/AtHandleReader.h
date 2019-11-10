#pragma once

#include "AtOvlReader.h"


namespace At
{

	class HandleReader : virtual public OvlReader
	{
	public:
		HandleReader() = default;
		HandleReader(HANDLE h) : m_h(h) {}

		void SetHandle(HANDLE h) { EnsureThrow(m_h == INVALID_HANDLE_VALUE); m_h = h; m_offset = 0; m_setOffset = (GetFileType(h) == FILE_TYPE_DISK); }
		void SetOffset(uint64 o) { EnsureThrow(m_h != INVALID_HANDLE_VALUE); m_offset = o; }

	protected:
		IoResult::E OvlReader_Read(void* buf, DWORD nrBytesToRead, DWORD& nrBytesRead, OVERLAPPED& ovl, int64& errorCode) override final;
		IoResult::E OvlReader_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesRead, bool wait, int64& errorCode) override final;
		void OvlReader_Cancel(OVERLAPPED& ovl) override final;

		HANDLE m_h         { INVALID_HANDLE_VALUE };
		uint64 m_offset    {};
		bool   m_setOffset {};

		struct State { enum E { Ready, Pending, Ended }; };
		State::E m_state { State::Ready };
	};

}
