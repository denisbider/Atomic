#pragma once

#include "AtWorkPoolThread.h"

namespace At
{
	template <class WorkPoolType>
	class WebServerThread : public WorkPoolThread<WorkPoolType>
	{
	protected:
		virtual void SendPreparedResponse(HttpRequest& req, HTTP_RESPONSE* resp) = 0;

	protected:
		struct CommonSendErr {};		// NOT derived from std::exception

		void ProcessRequest(Rp<WebRequestHandlerCreator> const& reqHandlerCreator, HttpRequest& req);

		void LogReqErr(Seq prologue, Seq handlerType, Seq errType, HttpRequest& req, Seq details);

		void SendBadRequest(HttpRequest& req, Seq text) { SendStatus(req, HttpStatus::BadRequest, text); }
		void SendInternalServerError(HttpRequest& req, Seq text) { SendStatus(req, HttpStatus::InternalServerError, text); }
		void SendStatus(HttpRequest& req, uint statusCode, Seq text);
		void SendResponse(HttpRequest& req, HTTP_RESPONSE* resp);
	};
}
