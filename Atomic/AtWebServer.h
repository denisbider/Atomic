#pragma once

#include "AtWebRequestHandlerCreator.h"
#include "AtWebRequestLog.h"
#include "AtWorkPool.h"


namespace At
{

	template <class ThreadType, class WorkItemType>
	class WebServer : public WorkPool<ThreadType, WorkItemType>
	{
	public:	
		virtual EntityStore& GetStore() = 0;

		// MUST be thread safe.
		// Creates a request handler for an incoming request.
		// MUST create a non-null request handler.
		// If it throws an exception, the request handling thread will exit without sending a response.
		// If the exception derives from std::exception, the exception description will be logged.
		virtual Rp<WebRequestHandlerCreator> WebServer_InitRequestHandlerCreator(HttpRequest& req) = 0;

	public:
		bool ShouldLogReqErr(Seq handlerType, Seq errType, sizet& nrUnlogged);
		WebRequestLog& RequestLog() { return m_requestLog; }

	protected:
		WebRequestLog m_requestLog;

	private:
		struct ReqErrKey
		{
			ReqErrKey(Seq handlerType, Seq errType) : m_handlerType(handlerType), m_errType(errType) {}
		
			Str m_handlerType;
			Str m_errType;
		
			bool operator== (ReqErrKey const& r) const { return m_handlerType == r.m_handlerType && m_errType == r.m_errType; }
			bool operator<  (ReqErrKey const& r) const { int diff { Seq(m_handlerType).Compare(r.m_handlerType, CaseMatch::Exact) }; return diff<0 || (!diff && m_errType<r.m_errType); }
		};

		struct ReqErrInfo
		{
			ReqErrInfo(uint64 lastLogResetTickCount, sizet nrLogged)
				: m_lastLogResetTickCount(lastLogResetTickCount), m_nrLogged(nrLogged) {}
	
			uint64 m_lastLogResetTickCount;
			sizet m_nrLogged;
			sizet m_nrUnlogged {};
		};

		typedef std::map<ReqErrKey, ReqErrInfo> ReqErrMap;

		Mutex m_reqErrMx;
		ReqErrMap m_reqErrMap;
		uint64 m_lastReqErrTickCount {};
	};

}
