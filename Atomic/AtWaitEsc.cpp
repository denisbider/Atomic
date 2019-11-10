#include "AtIncludes.h"
#include "AtWaitEsc.h"

#include "AtConsole.h"


namespace At
{

	void WaitEsc::ThreadMain()
	{
		while (true)
		{
			uint c { Console::ReadChar(m_stopCtl) };
			if (c == UINT_MAX || c == 27)
			{
				m_stopCtl->Stop("Esc key pressed");
				break;
			}
		}
	}

}
