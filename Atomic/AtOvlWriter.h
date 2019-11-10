#pragma once

#include "AtEvent.h"
#include "AtWriter.h"


namespace At
{
	class OvlWriter : virtual public Writer, virtual public Abortable
	{
	public:
		void SetWriteSize(DWORD writeSize) { m_writeSize = writeSize; }

		void Write(Seq data) override final;

	protected:
		struct IoResult { enum E { Done, Pending, Error }; }; 
		virtual IoResult::E OvlWriter_Write(void const* buf, DWORD nrBytesToWrite, DWORD& nrBytesWritten, OVERLAPPED& ovl, int64& errorCode) = 0;
		virtual IoResult::E OvlWriter_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesWritten, bool wait, int64& errorCode) = 0;
		virtual void OvlWriter_Cancel(OVERLAPPED& ovl) = 0;

		DWORD m_writeSize { 32000 };
		Event m_writeEvent { Event::CreateAuto };
		OVERLAPPED m_ovl;
	};
}
