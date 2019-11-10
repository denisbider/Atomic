#pragma once

#include "AtThread.h"


namespace At
{

	struct WaitEsc : public Thread
	{
		void ThreadMain();
	};

}
