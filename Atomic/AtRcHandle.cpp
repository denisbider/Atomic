#include "AtIncludes.h"
#include "AtRcHandle.h"

#include "AtWinErr.h"

namespace At
{
	RcHandle::~RcHandle()
	{
		if (IsValid())
			EnsureReportWithNr(CloseHandle(m_handle), GetLastError());
	}
}
