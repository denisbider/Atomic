#pragma once

#include "AtEvent.h"
#include "AtReader.h"


namespace At
{

	class OvlReader : virtual public Reader, virtual public Abortable
	{
	public:
		void SetReadSize(DWORD readSize) { m_readSize = readSize; }

		void Read(std::function<ReadInstr(Seq&)> process) override final;

	protected:
		struct IoResult { enum E { Done, Pending, ReachedEnd, Error }; }; 
		virtual IoResult::E OvlReader_Read(void* buf, DWORD nrBytesToRead, DWORD& nrBytesRead, OVERLAPPED& ovl, int64& errorCode) = 0;
		virtual IoResult::E OvlReader_GetOvlResult(OVERLAPPED& ovl, DWORD& nrBytesRead, bool wait, int64& errorCode) = 0;
		virtual void OvlReader_Cancel(OVERLAPPED& ovl) = 0;
	
		DWORD m_readSize { 32000 };
		Event m_readEvent { Event::CreateAuto };
		OVERLAPPED m_ovl;
	};

}
