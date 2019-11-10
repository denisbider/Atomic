#include "AtIncludes.h"

#include "AtWait.h"


namespace At
{

	DWORD WaitHandles(DWORD count, HANDLE* handles, DWORD milliseconds)
	{
		DWORD rc { WaitForMultipleObjects(count, handles, false, milliseconds) };
		if (rc == WAIT_FAILED)
			{ LastWinErr e; throw e.Make<>("Error in WaitForMultipleObjects"); }
		if ((rc == WAIT_TIMEOUT && milliseconds != INFINITE) || (rc < count))
			return rc;

		throw ErrWithCode<>(rc, "Unexpected return value from WaitForMultipleObjects");
	}

}
