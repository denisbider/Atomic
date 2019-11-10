#pragma once

#include "AtAbortable.h"


namespace At
{
	class Writer : virtual public IAbortable
	{
	public:
		struct Err : CommunicationErr { Err(Seq msg) : CommunicationErr(msg) {} };
		struct WriteWinErr : WinErr<Err> { WriteWinErr(int64 rc) : WinErr<Err>(rc, "Error writing data") {} };

		virtual void Write(Seq data) = 0;
		virtual void Shutdown() = 0;
	};
}
