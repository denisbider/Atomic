#pragma once

#include "AtHttpServer.h"
#include "AtWebServerThread.h"

namespace At
{
	class HttpServerThread : public WebServerThread<HttpServer>
	{
	public:
		HttpServerThread() : m_httpEvent(Event::CreateAuto) {}

	private:
		Event m_httpEvent;
		OVERLAPPED m_ovl;
		Str m_ioBuf;
		bool m_responseSent;
	
		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;

		// If response version is not yet set (is 0.0), sets version to 1.0 or 1.1, depending on request version.
		void SendPreparedResponse(HttpRequest& req, HTTP_RESPONSE* resp) override;
	};
}
