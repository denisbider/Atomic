#pragma once

#include "AtIpFairThrottle.h"
#include "AtMutex.h"
#include "AtObjectStore.h"
#include "AtWebServer.h"
#include "AtWorkPool.h"


namespace At
{

	class HttpServerThread;

	struct HttpServerWorkItem
	{
		Str m_reqBuf;
		bool m_moreData;
	};


	class HttpServer : public WebServer<HttpServerThread, HttpServerWorkItem>
	{
	protected:
		HttpServer() : m_httpHandle(INVALID_HANDLE_VALUE), m_httpEvent(Event::CreateAuto) {}
		~HttpServer();

		// Can be called from any thread. Adds new URLs to listen on. Removes URLs not on the list. Does not touch unchanged URLs.
		// Takes a NON-const reference so that URLs can be canonicalized. For example, https://www.example.com/ needs to be canonicalized
		// to https://www.example.com:443/, otherwise HttpAddUrl will not accept it.
		void SyncUrls(Vec<Str>& newUrls);

		// Below methods are called at initialization. HttpServer_SyncUrls gives the application an opportunity to make the first call to SyncUrls().
		// On exception, the web server work pool will exit. If the exception derives from std::exception, the exception description will be logged.
		virtual char const* HttpServer_LogNamePrefix() const = 0;
		virtual void HttpServer_SyncUrls() = 0;

	private:
		struct UrlState
		{
			Str m_url;
			bool m_presentInOld {};
			bool m_presentInNew {};
		};

		Mutex          m_mxUrls;
		bool           m_closed {};
		Vec<UrlState>  m_urls;

		enum { ReqBufSize = 65000 };
		HANDLE         m_httpHandle;
		Event          m_httpEvent;
		OVERLAPPED     m_ovl;
		Str            m_ioBuf;

		IpFairThrottle m_ipFairThrottle;
	
		void WorkPool_Run();

		friend class HttpServerThread;
	};

}
