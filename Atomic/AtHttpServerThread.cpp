#include "AtIncludes.h"
#include "AtHttpServerThread.h"

#include "AtNumCvt.h"
#include "AtHttpServer.h"
#include "AtWinErr.h"


namespace At
{

	void HttpServerThread::WorkPoolThread_ProcessWorkItem(void* pvWorkItem)
	{
		AutoFree<HttpServerWorkItem> workItem = (HttpServerWorkItem*) pvWorkItem;
		HttpRequest req((HTTP_REQUEST*) workItem->m_reqBuf.Ptr(), WebServerType::Http);

		m_responseSent = false;
		OnExit cancelRequest = [&]
			{
				if (!m_responseSent)
					HttpCancelHttpRequest(m_workPool->m_httpHandle, req.RequestId(), 0);
			};

		IpFairThrottle::HoldLocator ipThrottleHoldLocator;
		if (!m_workPool->m_ipFairThrottle.TryAcquireHold(req.RemoteAddr(), ipThrottleHoldLocator))
			return;

		OnExit releaseIpThrottleHold = [&]
			{ EnsureThrow(m_workPool->m_ipFairThrottle.ReleaseHold(ipThrottleHoldLocator)); };

		// Obtain request handler creator
		Rp<WebRequestHandlerCreator> reqHandlerCreator(m_workPool->WebServer_InitRequestHandlerCreator(req));

		// Start calculating size of entity body
		sizet totalBodySize = 0;
		sizet nrChunks = 0;
 		for (sizet i=0; i!=req->EntityChunkCount; ++i)
		{
			HTTP_DATA_CHUNK const& chunk(req->pEntityChunks[i]);
			EnsureThrow(chunk.DataChunkType == HttpDataChunkFromMemory);
			totalBodySize += chunk.FromMemory.BufferLength;
			++nrChunks;
		}

		if (totalBodySize > reqHandlerCreator->MaxBodySize())
		{
			SendBadRequest(req, "Entity body too large");
			return;
		}

		// Check for more entity body	
		enum { BodyChunkSize = 260000 };
		Vec<Str> moreBodyChunks;

		if (workItem->m_moreData || (req->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS))
		{
			while (true)
			{
				m_ioBuf.ResizeExact(BodyChunkSize);

				ZeroMemory(&m_ovl, sizeof(m_ovl));
				m_ovl.hEvent = m_httpEvent.Handle();
			
				ULONG bytesReceived = 0;
				DWORD ulRet = HttpReceiveRequestEntityBody(m_workPool->m_httpHandle, req->RequestId, 0,
					m_ioBuf.Ptr(), BodyChunkSize, &bytesReceived, &m_ovl);

				// HttpReceiveRequestEntityBody can also complete immediately with ulRet == NO_ERROR
				if (ulRet == ERROR_IO_PENDING)
				{
					if (Wait2(StopEvent().Handle(), m_httpEvent.Handle(), INFINITE) == 0)
						throw ExecutionAborted();

					if (GetOverlappedResult(m_workPool->m_httpHandle, &m_ovl, &bytesReceived, true))
						ulRet = NO_ERROR;
					else
						ulRet = GetLastError();
				}
				
				EnsureAbort(bytesReceived <= BodyChunkSize);
			
				if (bytesReceived != 0)
				{
					moreBodyChunks.Add().Swap(m_ioBuf);
					moreBodyChunks.Last().ResizeExact(bytesReceived);
				
					totalBodySize += bytesReceived;
					++nrChunks;

					if (totalBodySize > reqHandlerCreator->MaxBodySize())
					{
						SendBadRequest(req, "Entity body too large");
						return;
					}
				}

				if (ulRet == ERROR_HANDLE_EOF)
					break;
				if (ulRet != NO_ERROR)
					return;
			}
		}
		
		// Make the entity body available at a single memory location
		Seq body;
		Str bodyBuf;
		
		if (nrChunks != 0)
		{
			if (nrChunks == 1)
			{
				if (req->EntityChunkCount)
				{
					HTTP_DATA_CHUNK const& chunk(req->pEntityChunks[0]);
					body = Seq(chunk.FromMemory.pBuffer, chunk.FromMemory.BufferLength);
				}
				else
					body = moreBodyChunks[0];
			}
			else
			{
				bodyBuf.ResizeExact(totalBodySize);
				sizet bytesCopied = 0;
				for (sizet i=0; i!=req->EntityChunkCount; ++i)
				{
					HTTP_DATA_CHUNK const& chunk(req->pEntityChunks[i]);
					sizet chunkLen = chunk.FromMemory.BufferLength;
					EnsureAbort(bytesCopied + chunkLen <= bodyBuf.Len());
					memcpy(bodyBuf.Ptr() + bytesCopied, chunk.FromMemory.pBuffer, chunkLen);
					bytesCopied += chunkLen;
				}
				for (Str const& chunk : moreBodyChunks)
				{
					EnsureAbort(bytesCopied + chunk.Len() <= bodyBuf.Len());
					memcpy(bodyBuf.Ptr() + bytesCopied, chunk.Ptr(), chunk.Len());
					bytesCopied += chunk.Len();
				}
				EnsureAbort(bytesCopied == bodyBuf.Len());
				body = bodyBuf;
			}
		}

		req.SetBody(body);

		// Process request
		try { ProcessRequest(reqHandlerCreator, req); }
		catch (CommonSendErr const&) {}
	}


	void HttpServerThread::SendPreparedResponse(HttpRequest& req, HTTP_RESPONSE* resp)
	{
		ZeroMemory(&m_ovl, sizeof(m_ovl));
		m_ovl.hEvent = m_httpEvent.Handle();

		ULONG ulRet = HttpSendHttpResponse(m_workPool->m_httpHandle, req->RequestId, 0, resp, 0, 0, 0, 0, &m_ovl, 0);

		if (ulRet == ERROR_IO_PENDING)
		{
			if (Wait2(StopEvent().Handle(), m_httpEvent.Handle(), INFINITE) == 0)
				throw ExecutionAborted();

			ULONG bytesReceived = 0;
			if (GetOverlappedResult(m_workPool->m_httpHandle, &m_ovl, &bytesReceived, false))
				ulRet = NO_ERROR;
			else
				ulRet = GetLastError();
		}

		if (ulRet != NO_ERROR)
		{
			if (ulRet == ERROR_NETNAME_DELETED    ||		//   64 - "The specified network name is no longer available."
				ulRet == ERROR_CONNECTION_INVALID ||		// 1229 - "An operation was attempted on a nonexistent network connection."
				ulRet == ERROR_CONNECTION_ABORTED)			// 1236 - "The network connection was aborted by the local system."
				throw CommonSendErr();

			throw WinErr<>(ulRet, "Error in HttpSendHttpResponse");
		}

		m_responseSent = true;
	}

}
