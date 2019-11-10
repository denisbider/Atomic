#include "AtIncludes.h"
#include "AtInitOnFirstUse.h"


namespace At
{

	void InitOnFirstUse(LONG volatile* flag, std::function<void()> initFunc)
	{
		enum { INITED_THRESHOLD = 0x40000000 };
	
		if (*flag >= INITED_THRESHOLD)
		{
			// Already initialized
			return;
		}

		// Not yet initialized.
		LONG value = InterlockedIncrement(flag);
		if (value > 1)
		{
			// Being initialized by another thread. Wait for it to finish
			while (value < INITED_THRESHOLD)
			{
				// Spin with Sleep(0) is fine for this special case conflict scenario.
				Sleep(0);
				value = InterlockedExchangeAdd(flag, 0);
			}

			// Was just initialized by another thread
			return;
		}

		// It's this thread's job to initialize.
		initFunc();
		InterlockedExchangeAdd(flag, INITED_THRESHOLD);
	}

}
