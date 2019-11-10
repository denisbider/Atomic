#pragma once

#include "AtIncludes.h"


namespace At
{

	struct SpinLock : NoCopy
	{
		void Acquire()
		{
			while (InterlockedCompareExchange(&m_flag, 1, 0) != 0)
				SwitchToThread();
		}
		
		void Release()
		{
			m_flag = 0;
		}

	private:
		LONG volatile m_flag {};
	};

}
