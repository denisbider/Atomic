#include "AtIncludes.h"
#include "AtBhtpnServerThread.h"
#include "AtHttpServerThread.h"


namespace At
{

	template <class WorkPoolType>
	void WebServerThread<WorkPoolType>::ProcessRequest(Rp<WebRequestHandlerCreator> const& reqHandlerCreator, HttpRequest& req)
	{
		// Log request
		m_workPool->RequestLog().OnRequest(req);
	
		// Process request
		bool errorProcessing {};
		EntityStore& store { m_workPool->GetStore() };
		Rp<WebRequestHandler> reqHandler;
		ReqResult reqResult {};
		char const* errMsg = "Error processing request";

		auto getHandlerType = [&] () -> char const*
			{
				if (!reqHandler.Any()) return "(null)";
				return typeid(reqHandler.Ref()).name();
			};

		try
		{
			struct AbortTxOnSuccess {};

			auto tx = [&] ()
				{
					reqHandler.Set(reqHandlerCreator->Create());
					reqResult = reqHandler->Process(store, req);
					if (reqHandler->m_abortTxOnSuccess)
						throw AbortTxOnSuccess();
				};

			try
			{
				ReqTxExclusive::E rtx { reqHandlerCreator->ReqTxExclusivity() };
				if ( ReqTxExclusive::Always == rtx ||
					(ReqTxExclusive::IfPost == rtx && req.IsPost()))
					store.RunTxExclusive(tx);
				else
					store.RunTx(GetStopCtl(), typeid(reqHandlerCreator.Ref()), tx);
			}
			catch (AbortTxOnSuccess const&) {}

			if (ReqResult::Continue == reqResult)
			{
				errMsg = "Request processed. Error generating response";
				reqHandler->GenResponse(req);
			}

			if (req.IsSecure())
			{
				// As of August 2017, this appears to be the best universal policy. If one connection is secure,
				// assume it is a policy of the entire domain to have security, and to have it indefinitely.
				// The max-age parameter, 99979997, equals approximately 3 years and 2 months. Reasons for this value:
				// - The commonly used value, 31536000, amounts to only one year. This seems a bit short.
				// - 99979997 is more memorable, and easier to visually verify for correctness (two groups of four digits).
				// January 2018: Added "preload". This allows browsers to hardcode strict transport security for this domain.
				reqHandler->SetResponseHeader_StrictTransportSecurity("max-age=99979997; includeSubDomains; preload");
			}
		}
		catch (std::exception const& e)
		{
			LogReqErr(errMsg, getHandlerType(), typeid(e).name(), req, e.what());
			errorProcessing = true;
		}

		try
		{
			if (errorProcessing)
				SendInternalServerError(req, errMsg);
			else
				SendResponse(req, &reqHandler->m_response);
		}
		catch (CommonSendErr  const&  ) { throw; }
		catch (std::exception const& e) { LogReqErr("Error sending response", getHandlerType(), typeid(e).name(), req, e.what()); }
	}


	template <class WorkPoolType>
	void WebServerThread<WorkPoolType>::LogReqErr(Seq prologue, Seq handlerType, Seq errType, HttpRequest& req, Seq details)
	{
		sizet nrUnlogged = 0;
		bool shouldLog = m_workPool->ShouldLogReqErr(handlerType, errType, nrUnlogged);

		if (shouldLog)
		{
			Str msg = Str::Join(
				prologue, ": \r\n"
				"Handler type: ", handlerType, "\r\n"
				"Error type: ", errType, "\r\n");

			if (nrUnlogged)
				msg.Add(Str("Unlogged similar errors since last log: ").UInt(nrUnlogged).Add("\r\n"));

			msg.Add("URI: ").Add(Seq(req->pRawUrl, req->RawUrlLength)).Add("\r\n\r\n").Add(details);

			WorkPool_LogEvent(EVENTLOG_WARNING_TYPE, msg);
		}
	}


	template <class WorkPoolType>
	void WebServerThread<WorkPoolType>::SendStatus(HttpRequest& req, uint statusCode, Seq text)
	{
		Seq contentType("text/plain; charset=UTF-8");
		Str content;
		content.UInt(statusCode).Add(" ").Add(text);

		HTTP_DATA_CHUNK chunk {};
		chunk.DataChunkType = HttpDataChunkFromMemory;
		chunk.FromMemory.BufferLength = NumCast<ULONG>(content.Len());
		chunk.FromMemory.pBuffer = (PVOID) content.Ptr();

		HTTP_UNKNOWN_HEADER hdrXFO {};
		hdrXFO.pName = "X-Frame-Options";
		hdrXFO.NameLength = 15;
		hdrXFO.pRawValue = "SAMEORIGIN";
		hdrXFO.RawValueLength = 10;

		HTTP_RESPONSE resp {};
		resp.StatusCode = NumCast<USHORT>(statusCode);
		resp.ReasonLength = NumCast<USHORT>(text.n);
		resp.pReason = (PCSTR) text.p;
		resp.EntityChunkCount = 1;
		resp.pEntityChunks = &chunk;
		resp.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = (PCSTR) contentType.p;
		resp.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength = NumCast<USHORT>(contentType.n);
		resp.Headers.UnknownHeaderCount = 1;
		resp.Headers.pUnknownHeaders = &hdrXFO;
	
		SendResponse(req, &resp);
	}


	template <class WorkPoolType>
	void WebServerThread<WorkPoolType>::SendResponse(HttpRequest& req, HTTP_RESPONSE* resp)
	{
		// Set HTTP Version
		if (resp->Version.MajorVersion == 0 && resp->Version.MinorVersion == 0)
		{
			resp->Version.MajorVersion = 1;

			if (req->Version.MajorVersion == 1 && req->Version.MinorVersion <= 1)
				resp->Version.MinorVersion = req->Version.MinorVersion;
			else
			{
				if (req->Version.MajorVersion < 1)
					resp->Version.MinorVersion = 0;
				else
					resp->Version.MinorVersion = 1;
			}
		}

		// Set reason phrase
		if (resp->pReason == nullptr || resp->ReasonLength == 0)
		{
			Seq desc = HttpStatus::Describe(resp->StatusCode);
			resp->pReason = (PCSTR) desc.p;
			resp->ReasonLength = NumCast<USHORT>(desc.n);
		}

		// Set Content-Length
		bool haveContentLength = true;
		uint64 contentLength = 0;
		for (USHORT i=0; haveContentLength && i!=resp->EntityChunkCount; ++i)
		{
			HTTP_DATA_CHUNK const& chunk = resp->pEntityChunks[i];
			switch (chunk.DataChunkType)
			{
			case HttpDataChunkFromMemory:
				contentLength += chunk.FromMemory.BufferLength;
				break;
		
			case HttpDataChunkFromFileHandle:
				if (chunk.FromFileHandle.ByteRange.Length.QuadPart != HTTP_BYTE_RANGE_TO_EOF)
					contentLength += chunk.FromFileHandle.ByteRange.Length.QuadPart;
				else
				{
					LARGE_INTEGER fileSize; 
					if (!GetFileSizeEx(chunk.FromFileHandle.FileHandle, &fileSize))
						haveContentLength = false;
					else
					{
						if (NumCast<ULONGLONG>(fileSize.QuadPart) > chunk.FromFileHandle.ByteRange.StartingOffset.QuadPart)
							contentLength += fileSize.QuadPart - chunk.FromFileHandle.ByteRange.StartingOffset.QuadPart;
					}
				}
				break;
		
			default: EnsureThrow(!"Unsupported chunk type");
			}
		}
	
		// Buffer must remain valid until response is sent
		Str contentLengthBuf;
		if (haveContentLength)
		{
			contentLengthBuf.UInt(contentLength);
			resp->Headers.KnownHeaders[HttpHeaderContentLength].pRawValue = (PCSTR) contentLengthBuf.Ptr();
			resp->Headers.KnownHeaders[HttpHeaderContentLength].RawValueLength = NumCast<USHORT>(contentLengthBuf.Len());
		}
	
		// Clear content body if HTTP verb is HEAD
		if (req->Verb == HttpVerbHEAD)
		{
			resp->EntityChunkCount = 0;
			resp->pEntityChunks = nullptr;
		}

		// Log our response
		m_workPool->RequestLog().OnResponse(req, resp, haveContentLength, contentLength);

		// Send response
		SendPreparedResponse(req, resp);
	}


	template class WebServerThread<BhtpnServer>;
	template class WebServerThread<HttpServer>;

}
