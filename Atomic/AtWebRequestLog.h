#pragma once

#include "AtStr.h"
#include "AtMutex.h"
#include "AtHttpRequest.h"
#include "AtTextLog.h"


namespace At
{

	class WebRequestLog : public TextLog
	{
	public:
		void OnRequest(HttpRequest& req);
		void OnResponse(HttpRequest const& req, HTTP_RESPONSE* resp, bool haveContentLength, uint64 contentLength);
	};

}
