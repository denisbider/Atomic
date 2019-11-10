#include "AtIncludes.h"
#include "AtBhtpnServer.h"
#include "AtHttpServer.h"

#include "AtDllKernel32.h"


namespace At
{

	template <class ThreadType, class WorkItemType>
	bool WebServer<ThreadType, WorkItemType>::ShouldLogReqErr(Seq handlerType, Seq errType, sizet& nrUnlogged)
	{
		bool shouldLog = true;
		uint64 curTickCount = E_GetTickCount64();

		nrUnlogged = SIZE_MAX;

		Locker locker(m_reqErrMx);
	
		if (curTickCount < m_lastReqErrTickCount)
		{
			// System time moved back
			uint64 diff = m_lastReqErrTickCount - curTickCount;
			if (diff > 120ULL * 1000ULL)
			{
				// Discrepancy larger than 2 minutes, clear state
				m_reqErrMap.clear();
			}
			else
			{
				// Discrepancy smaller than 2 minutes, pretend that time is stuck for a while
				curTickCount = m_lastReqErrTickCount;
			}
		}

		ReqErrKey key(handlerType, errType);
		ReqErrMap::iterator it = m_reqErrMap.find(key);
		if (it == m_reqErrMap.end())
		{
			m_reqErrMap.insert(std::make_pair(key, ReqErrInfo(curTickCount, 1)));
			nrUnlogged = 0;
		}
		else
		{
			// Log up to 3 request errors for same handler and of same class per a period of 6 hours
			if (SatSub(curTickCount, it->second.m_lastLogResetTickCount) > 6ULL * 3600ULL * 1000ULL)
			{
				// More than 6 hours elapsed since last log reset for this ReqErrKey. Do a reset
				nrUnlogged = it->second.m_nrUnlogged;

				it->second.m_lastLogResetTickCount = curTickCount;
				it->second.m_nrLogged = 1;
				it->second.m_nrUnlogged = 0;
			}
			else
			{
				// Less than 6 hours elapsed since last log reset for this ReqErrKey
				shouldLog = (it->second.m_nrLogged < 3);
				if (!shouldLog)
				{
					nrUnlogged = ++(it->second.m_nrUnlogged);
				}
				else
				{
					++(it->second.m_nrLogged);

					nrUnlogged = it->second.m_nrUnlogged;
					it->second.m_nrUnlogged = 0;
				}
			}
		}
	
		EnsureAbort(nrUnlogged != SIZE_MAX);
		m_lastReqErrTickCount = curTickCount;
		return shouldLog;
	}


	template class WebServer<BhtpnServerThread, BhtpnServerWorkItem>;
	template class WebServer<HttpServerThread, HttpServerWorkItem>;

}
