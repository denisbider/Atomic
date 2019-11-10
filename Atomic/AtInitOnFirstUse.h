#pragma once

#include "AtIncludes.h"


namespace At
{

	// flag must be initialized to zero and must not be changed outside of this function.
	void InitOnFirstUse(LONG volatile* flag, std::function<void()> initFunc);

}
