#include "AtIncludes.h"
#include "AtHttpServer.h"

#include "AtDllKernel32.h"
#include "AtHttpServerThread.h"
#include "AtNumCvt.h"
#include "AtUriGrammar.h"
#include "AtUtfWin.h"
#include "AtWebEntities.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	HttpServer::~HttpServer()
	{
		if (m_httpHandle != 0 && m_httpHandle != INVALID_HANDLE_VALUE)
		{
			CancelIoEx_or_CancelIo(m_httpHandle, &m_ovl);

			ULONG bytesReceived = 0;
			GetOverlappedResult(m_httpHandle, &m_ovl, &bytesReceived, TRUE);

			CloseHandle(m_httpHandle);
		}
	}


	void HttpServer::SyncUrls(Vec<Str>& newUrls)
	{
		Locker locker(m_mxUrls);

		if (!m_closed)
		{
			// Investigate URL state

			for (Str& newUrl : newUrls)
			{
				// Canonicalize URL: add port if not present, otherwise HttpAddUrl may reject URL
				ParseTree pt { newUrl };
				if (pt.Parse(Uri::C_URI_reference))
				{
					ParseNode const* schemeNode { pt.Root().FrontFind(Uri::id_scheme) };
					if (schemeNode)
					{
						uint defPort {};
						     if (schemeNode->SrcText().EqualInsensitive("https")) defPort = 443;
						else if (schemeNode->SrcText().EqualInsensitive("http" )) defPort = 80;

						if (defPort)
						{
							ParseNode const* serverNode { pt.Root().DeepFind(Uri::id_server) };
							if (serverNode)
							{
								ParseNode const* portNode { serverNode->FlatFind(Uri::id_port) };
								if (!portNode)
								{
									// Have HTTPS or HTTP URL with server, but no port. Add default port.
									Str urlCanon;
									urlCanon.ReserveExact(newUrl.Len() + 6)
										.Add(serverNode->BeforeRemaining())
										.Ch(':').UInt(defPort)
										.Add(serverNode->Remaining());

									newUrl.Set(std::move(urlCanon));
								}
							}
						}
					}
				}

				// See if URL exists in old state
				bool foundInOld {};
				for (UrlState& urlState : m_urls)
					if (Seq(newUrl).EqualInsensitive(urlState.m_url))
					{
						urlState.m_presentInNew = true;
						foundInOld = true;
						break;
					}

				if (!foundInOld)
				{
					m_urls.Add().m_url = ToLower(newUrl);
					m_urls.Last().m_presentInNew = true;
				}
			}

			// Apply URL state

			sizet i {};
			while (i != m_urls.Len())
			{
				UrlState& urlState { m_urls[i] };
				if (!urlState.m_presentInNew)
				{
					// Remove URL
					ULONG rc = HttpRemoveUrl(m_httpHandle, WinStr(urlState.m_url).Z());
					if (rc != NO_ERROR)
						WorkPool_LogEvent(EVENTLOG_ERROR_TYPE,
							Str("Error in HttpRemoveUrl for '").Add(urlState.m_url).Add("': ").Fun(DescribeWinErr, rc));

					m_urls.Erase(i, 1);
				}
				else
				{
					bool errorAddingUrl {};
					if (!urlState.m_presentInOld)
					{
						// Add URL
						ULONG rc = HttpAddUrl(m_httpHandle, WinStr(urlState.m_url).Z(), 0);
						if (rc == NO_ERROR)
							urlState.m_presentInOld = true;		// Preparation for next time SyncUrls() is called
						else
						{
							errorAddingUrl = true;

							if (rc != ERROR_ACCESS_DENIED)
								WorkPool_LogEvent(EVENTLOG_ERROR_TYPE,
									Str("Error in HttpAddUrl for '").Add(urlState.m_url).Add("': ").Fun(DescribeWinErr, rc));
							else
								WorkPool_LogEvent(EVENTLOG_ERROR_TYPE,
									Str("Access denied in HttpAddUrl for '").Add(urlState.m_url).Add("'.\r\n"
										"To grant a user permission to listen on a URL, run:\r\n"
										"netsh http add urlacl url=... user=... listen=yes"));
						}
					}

					if (errorAddingUrl)
						m_urls.Erase(i, 1);
					else
					{
						urlState.m_presentInNew = false;		// Preparation for next time SyncUrls() is called
						++i;
					}
				}
			}
		}
	}


	void HttpServer::WorkPool_Run()
	{
		// Initialize HTTP API
		HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
		ULONG httpInitializeFlags = HTTP_INITIALIZE_SERVER;
		ULONG ulRet = HttpInitialize(httpApiVersion, httpInitializeFlags, 0);
		if (ulRet != NO_ERROR)
			{ LastWinErr e; throw e.Make<>("Error in HttpInitialize"); }
	
		OnExit httpTerminate = [=] { HttpTerminate(httpInitializeFlags, (void*) 0); };
	
		// Create HTTP handle
		ulRet = HttpCreateHttpHandle(&m_httpHandle, 0);
		if (ulRet != NO_ERROR)
			{ LastWinErr e; throw e.Make<>("Error in HttpCreateHttpHandle"); }
	
		OnExit closeHttpHandle = [&] { Locker locker(m_mxUrls); m_closed = true; CloseHandle(m_httpHandle); };

		// Initialize IP throttle
		m_ipFairThrottle.SetMaxHoldsBySingleIp(32);
		m_ipFairThrottle.SetMaxHoldsGlobal(128 * std::thread::hardware_concurrency());

		// Init log
		m_requestLog.Init(Str(HttpServer_LogNamePrefix()).Add("HttpLog"));

		// Give application opportunity to call SyncUrls()
		HttpServer_SyncUrls();

		// Handle requests
		while (true)
		{
			if (StopEvent().IsSignaled())
				break;

			m_ioBuf.ResizeExact(ReqBufSize);

			ZeroMemory(&m_ovl, sizeof(m_ovl));
			m_ovl.hEvent = m_httpEvent.Handle();
		
			HTTP_REQUEST* req = (HTTP_REQUEST*) m_ioBuf.Ptr();
			ulRet = HttpReceiveHttpRequest(m_httpHandle, HTTP_NULL_ID, HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY, req, ReqBufSize, 0, &m_ovl);
			if (ulRet == ERROR_IO_PENDING)
			{
				if (Wait2(StopEvent().Handle(), m_httpEvent.Handle(), INFINITE) == 0)
					break;

				ULONG bytesReceived = 0;
				if (GetOverlappedResult(m_httpHandle, &m_ovl, &bytesReceived, true))
					ulRet = NO_ERROR;
				else
					ulRet = GetLastError();
			}

			AutoFree<HttpServerWorkItem> workItem = new HttpServerWorkItem;
			workItem->m_reqBuf.Swap(m_ioBuf);

			if (ulRet == NO_ERROR)
				workItem->m_moreData = false;
			else if (ulRet == ERROR_MORE_DATA)
				workItem->m_moreData = true;
			else
				throw WinErr<>(ulRet, "Error in HttpReceiveHttpRequest");

			EnqueueWorkItem(workItem);
		}
	}

}
