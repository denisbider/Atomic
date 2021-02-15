#include "AtIncludes.h"
#include "AtBhtpnServerThread.h"

#include "AtUriGrammar.h"


namespace At
{

	void BhtpnServerThread::WorkPoolThread_ProcessWorkItem(void* pvWorkItem)
	{
		enum { MaxTotalHeaderBytes = 64000, MaxNrHeaders = MaxTotalHeaderBytes / 2, MaxUriLen = 4000, MaxMethodLen = 100 };

		AutoFree<BhtpnServerWorkItem> workItem = (BhtpnServerWorkItem*) pvWorkItem;

		m_reader = new HandleReader(workItem->m_sh->Handle());
		OnExit freeReader([&] () { delete m_reader; m_reader = nullptr; });
		m_reader->SetStopCtl(*this);

		m_writer = new HandleWriter(workItem->m_sh->Handle());
		OnExit freeWriter([&] () { delete m_writer; m_writer = nullptr; });
		m_writer->SetStopCtl(*this);

		SOCKADDR_IN saNull {};
		saNull.sin_family = AF_INET;

		// Process requests
		try
		{
			while (true)
			{
				m_reader->SetExpireMs(IoTimeoutMs);
				m_reader->PinBuffer(MaxTotalHeaderBytes);

				// This will hold the currently pinned reader buffer, eventually.
				// Must be declared here so that it's destroyed AFTER all that we decode that's going to point to it
				Str hdrBuf;

				// Read request
				Seq requestUri, method;
				uint64 nrHeaders;

				m_reader->ReadVarStr   (requestUri, MaxUriLen   );
				m_reader->ReadVarStr   (method,     MaxMethodLen);
				m_reader->ReadVarUInt64(nrHeaders               );
				if (nrHeaders > MaxNrHeaders)
					return;

				ValidState vs;
				CookedUrlStorage cus;
				Vec<HTTP_UNKNOWN_HEADER> uhdrs;

				HTTP_REQUEST hreq {};
				HttpReq_SetVerb(hreq, vs, method);
				HttpReq_SetUrl(hreq, vs, cus, requestUri);
				hreq.Address.pLocalAddress = (PSOCKADDR) &saNull;
				hreq.Address.pRemoteAddress = (PSOCKADDR) &saNull;

				sizet nrHeadersRead = 0;
				while (nrHeadersRead != nrHeaders)
				{
					Seq name, value;
					m_reader->ReadVarStr(name);
					m_reader->ReadVarStr(value);
					HttpReq_SetHeader(hreq, vs, uhdrs, name, value);
					++nrHeadersRead;
				}

				// Validate request
				HttpRequest req(&hreq, WebServerType::Bhtpn);
				if (!vs.m_ok)
					SendBadRequest(req, vs.m_failReason);
				else
				{
					// Set aside header buffer
					m_reader->SwapOutProcessed(hdrBuf);
					m_reader->UnpinBuffer();

					// Obtain request handler creator
					Rp<WebRequestHandlerCreator> reqHandlerCreator(m_workPool->WebServer_InitRequestHandlerCreator(req));

					// Receive request body
					Vec<Str> bodyChunks;
					sizet bodyBytesReceived = 0;

					while (true)
					{
						// Read chunk size
						uint64 chunkSize = 0;
						m_reader->ReadVarUInt64(chunkSize);
					
						// Last chunk?
						if (chunkSize == 0)
							break;

						// Body size too large?
						if (bodyBytesReceived + chunkSize > reqHandlerCreator->MaxBodySize())
							return;		// If we try to send a response, we get into deadlock with client (both sides want to write, neither reads)

						// Read chunk data
						sizet chunkBytesToGo = (sizet) chunkSize;
						do
						{
							m_reader->Read(
								[&] (Seq& data) -> Reader::ReadInstr
								{
									if (data.n)
									{
										Seq subChunk = data.ReadBytes(chunkBytesToGo);
										bodyChunks.Add() = subChunk;
										chunkBytesToGo -= subChunk.n;
										bodyBytesReceived += subChunk.n;

										// Received some data. Reset reader timeout
										m_reader->SetExpireMs(IoTimeoutMs);
									}
									return Reader::ReadInstr::Done;
								} );
						}
						while (chunkBytesToGo > 0);
					}

					// Make body available at single memory location
					Seq body;
					Str bodyBuf;
				
					if (bodyChunks.Any())
					{
						if (bodyChunks.Len() == 1)
							body = bodyChunks[0];
						else
						{
							bodyBuf.ResizeExact(bodyBytesReceived);
							sizet bytesCopied = 0;
							for (Str const& chunk : bodyChunks)
							{
								EnsureAbort(bytesCopied + chunk.Len() <= bodyBuf.Len());
								memcpy(bodyBuf.Ptr() + bytesCopied, chunk.Ptr(), chunk.Len());
								bytesCopied += chunk.Len();
							}
							body = bodyBuf;
						}
					}

					req.SetBody(body);

					// Process request
					ProcessRequest(reqHandlerCreator, req);
				}
			}
		}
		catch (CommonSendErr    const&) {}
		catch (CommunicationErr const&) {}
		catch (InputErr         const&) {}
	}


	void BhtpnServerThread::HttpReq_SetVerb(HTTP_REQUEST& hreq, ValidState& vs, Seq verb)
	{
		if (vs.m_ok)
		{
				 if (verb.EqualInsensitive("OPTIONS"  )) hreq.Verb = HttpVerbOPTIONS;
			else if (verb.EqualInsensitive("GET"      )) hreq.Verb = HttpVerbGET;
			else if (verb.EqualInsensitive("HEAD"     )) hreq.Verb = HttpVerbHEAD;
			else if (verb.EqualInsensitive("POST"     )) hreq.Verb = HttpVerbPOST;
			else if (verb.EqualInsensitive("PUT"      )) hreq.Verb = HttpVerbPUT;
			else if (verb.EqualInsensitive("DELETE"   )) hreq.Verb = HttpVerbDELETE;
			else if (verb.EqualInsensitive("TRACE"    )) hreq.Verb = HttpVerbTRACE;
			else if (verb.EqualInsensitive("CONNECT"  )) hreq.Verb = HttpVerbCONNECT;
			else if (verb.EqualInsensitive("TRACK"    )) hreq.Verb = HttpVerbTRACK;
			else if (verb.EqualInsensitive("MOVE"     )) hreq.Verb = HttpVerbMOVE;
			else if (verb.EqualInsensitive("COPY"     )) hreq.Verb = HttpVerbCOPY;
			else if (verb.EqualInsensitive("PROPFIND" )) hreq.Verb = HttpVerbPROPFIND;
			else if (verb.EqualInsensitive("PROPPATCH")) hreq.Verb = HttpVerbPROPPATCH;
			else if (verb.EqualInsensitive("MKCOL"    )) hreq.Verb = HttpVerbMKCOL;
			else if (verb.EqualInsensitive("LOCK"     )) hreq.Verb = HttpVerbLOCK;
			else if (verb.EqualInsensitive("UNLOCK"   )) hreq.Verb = HttpVerbUNLOCK;
			else if (verb.EqualInsensitive("SEARCH"   )) hreq.Verb = HttpVerbSEARCH;
			else
			{
				hreq.pUnknownVerb = (char const*) verb.p;
				hreq.UnknownVerbLength = NumCast<USHORT>(verb.n);
				hreq.Verb = HttpVerbUnknown;
			}
		}
	}


	void BhtpnServerThread::HttpReq_SetUrl(HTTP_REQUEST& hreq, ValidState& vs, CookedUrlStorage& cus, Seq rawUrl)
	{
		if (vs.m_ok)
		{
			hreq.pRawUrl = (char const*) rawUrl.p;
			hreq.RawUrlLength = NumCast<USHORT>(rawUrl.n);

			ParseTree parseTree(rawUrl);
			if (!parseTree.Parse(Uri::C_URI_reference))
				vs.Fail("Could not parse raw request URI");
			else
			{
				if (parseTree.Root().DeepFind(Uri::id_rel_path) != nullptr)
					vs.Fail("Request URI contains a relative path");
				else
				{
					ParseNode const* absPathNode = parseTree.Root().DeepFind(Uri::id_abs_path);
					if (!absPathNode)
						vs.Fail("No absolute path found in request URI");
					else
					{
						Str canonUrl = Str::Join("bhtpn://", m_workPool->GetPipeName(), ".l", absPathNode->SrcTextAndRemaining());
						HttpReq_SetCookedUrl(hreq, vs, cus, canonUrl);
					}
				}
			}
		}
	}


	void BhtpnServerThread::HttpReq_SetCookedUrl(HTTP_REQUEST& hreq, ValidState& vs, CookedUrlStorage& cus, Seq canonUrl)
	{
		cus.m_fullUrl.Set(canonUrl);
		hreq.CookedUrl.pFullUrl = cus.m_fullUrl.Z();
		hreq.CookedUrl.FullUrlLength = NumCast<USHORT>(sizeof(wchar_t) * cus.m_fullUrl.Len());

		ParseTree parseTree(canonUrl);
		if (!parseTree.Parse(Uri::C_URI_reference))
			vs.Fail("Could not parse canonicalized request URI");
		else
		{
			ParseNode const* serverNode = parseTree.Root().DeepFind(Uri::id_server);
			if (!serverNode)
				vs.Fail("Request URI missing host part");
			else
			{
				cus.m_host.Set(serverNode->SrcText());
				hreq.CookedUrl.pHost = cus.m_host.Z();
				hreq.CookedUrl.HostLength = NumCast<USHORT>(sizeof(wchar_t) * cus.m_host.Len());

				ParseNode const* absPathNode = parseTree.Root().DeepFind(Uri::id_abs_path);
				if (!absPathNode)
					vs.Fail("Request URI missing absolute path");
				else
				{
					cus.m_absPath.Set(absPathNode->SrcText());
					hreq.CookedUrl.pAbsPath = cus.m_absPath.Z();
					hreq.CookedUrl.AbsPathLength = NumCast<USHORT>(sizeof(wchar_t) * cus.m_absPath.Len());

					ParseNode const* queryStringNode = parseTree.Root().DeepFind(Uri::id_QM_query);
					if (queryStringNode != nullptr)
					{
						cus.m_queryString.Set(queryStringNode->SrcText());
						hreq.CookedUrl.pQueryString = cus.m_queryString.Z();
						hreq.CookedUrl.QueryStringLength = NumCast<USHORT>(sizeof(wchar_t) * cus.m_queryString.Len());
					}
				}
			}
		}
	}


	void BhtpnServerThread::HttpReq_SetHeader(HTTP_REQUEST& hreq, ValidState& vs, Vec<HTTP_UNKNOWN_HEADER>& uhdrs, Seq name, Seq value)
	{
		if (vs.m_ok)
		{
			HTTP_HEADER_ID headerId;
			if (GetKnownRequestHeaderId(headerId, name))
			{
				HTTP_KNOWN_HEADER& khdr = hreq.Headers.KnownHeaders[headerId];
				khdr.pRawValue = (char const*) value.p;
				khdr.RawValueLength = NumCast<USHORT>(value.n);
			}
			else
			{
				uhdrs.Add().pName = (char const*) name.p;
				uhdrs.Last().NameLength = NumCast<USHORT>(name.n);
				uhdrs.Last().pRawValue = (char const*) value.p;
				uhdrs.Last().RawValueLength = NumCast<USHORT>(value.n);

				hreq.Headers.pUnknownHeaders = uhdrs.Ptr();
				hreq.Headers.UnknownHeaderCount = NumCast<USHORT>(uhdrs.Len());
			}
		}
	}


	BhtpnServerThread::EnumInstr::E BhtpnServerThread::EnumKnownCommonHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f)
	{
		if (f("Cache-Control",       HttpHeaderCacheControl      ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Connection",          HttpHeaderConnection        ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Date",                HttpHeaderDate              ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Keep-Alive",          HttpHeaderKeepAlive         ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Pragma",              HttpHeaderPragma            ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Trailer",             HttpHeaderTrailer           ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Transfer-Encoding",   HttpHeaderTransferEncoding  ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Upgrade",             HttpHeaderUpgrade           ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Via",                 HttpHeaderVia               ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Warning",             HttpHeaderWarning           ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Allow",               HttpHeaderAllow             ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Length",      HttpHeaderContentLength     ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Type",        HttpHeaderContentType       ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Encoding",    HttpHeaderContentEncoding   ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Language",    HttpHeaderContentLanguage   ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Location",    HttpHeaderContentLocation   ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-MD5",         HttpHeaderContentMd5        ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Content-Range",       HttpHeaderContentRange      ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Expires",             HttpHeaderExpires           ) == EnumInstr::Stop) return EnumInstr::Stop;
		if (f("Last-Modified",       HttpHeaderLastModified      ) == EnumInstr::Stop) return EnumInstr::Stop;
		return EnumInstr::Continue;
	}


	void BhtpnServerThread::EnumKnownRequestHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f)
	{
		if (EnumKnownCommonHeaders(f) == EnumInstr::Stop) return;
			
		if (f("Accept",              HttpHeaderAccept            ) == EnumInstr::Stop) return;
		if (f("Accept-Charset",      HttpHeaderAcceptCharset     ) == EnumInstr::Stop) return;
		if (f("Accept-Encoding",     HttpHeaderAcceptEncoding    ) == EnumInstr::Stop) return;
		if (f("Accept-Language",     HttpHeaderAcceptLanguage    ) == EnumInstr::Stop) return;
		if (f("Authorization",       HttpHeaderAuthorization     ) == EnumInstr::Stop) return;
		if (f("Cookie",              HttpHeaderCookie            ) == EnumInstr::Stop) return;
		if (f("Expect",              HttpHeaderExpect            ) == EnumInstr::Stop) return;
		if (f("From",                HttpHeaderFrom              ) == EnumInstr::Stop) return;
		if (f("Host",                HttpHeaderHost              ) == EnumInstr::Stop) return;
		if (f("If-Match",            HttpHeaderIfMatch           ) == EnumInstr::Stop) return;
		if (f("If-Modified-Since",   HttpHeaderIfModifiedSince   ) == EnumInstr::Stop) return;
		if (f("If-None-Match",       HttpHeaderIfNoneMatch       ) == EnumInstr::Stop) return;
		if (f("If-Range",            HttpHeaderIfRange           ) == EnumInstr::Stop) return;
		if (f("If-Unmodified-Since", HttpHeaderIfUnmodifiedSince ) == EnumInstr::Stop) return;
		if (f("Max-Forwards",        HttpHeaderMaxForwards       ) == EnumInstr::Stop) return;
		if (f("Proxy-Authorization", HttpHeaderProxyAuthorization) == EnumInstr::Stop) return;
		if (f("Referer",             HttpHeaderReferer           ) == EnumInstr::Stop) return;
		if (f("Range",               HttpHeaderRange             ) == EnumInstr::Stop) return;
		if (f("TE",                  HttpHeaderTe                ) == EnumInstr::Stop) return;
		if (f("Translate",           HttpHeaderTranslate         ) == EnumInstr::Stop) return;
		if (f("User-Agent",          HttpHeaderUserAgent         ) == EnumInstr::Stop) return;
	}


	void BhtpnServerThread::EnumKnownResponseHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f)
	{
		if (EnumKnownCommonHeaders(f) == EnumInstr::Stop) return;

		if (f("Accept-Ranges",      HttpHeaderAcceptRanges     ) == EnumInstr::Stop) return;
		if (f("Age",                HttpHeaderAge              ) == EnumInstr::Stop) return;
		if (f("ETag",               HttpHeaderEtag             ) == EnumInstr::Stop) return;
		if (f("Location",           HttpHeaderLocation         ) == EnumInstr::Stop) return;
		if (f("Proxy-Authenticate", HttpHeaderProxyAuthenticate) == EnumInstr::Stop) return;
		if (f("Retry-After",        HttpHeaderRetryAfter       ) == EnumInstr::Stop) return;
		if (f("Server",             HttpHeaderServer           ) == EnumInstr::Stop) return;
		if (f("Set-Cookie",         HttpHeaderSetCookie        ) == EnumInstr::Stop) return;
		if (f("Vary",               HttpHeaderVary             ) == EnumInstr::Stop) return;
		if (f("WWW-Authenticate",   HttpHeaderWwwAuthenticate  ) == EnumInstr::Stop) return;
	}


	void BhtpnServerThread::EnumResponseHeaders(HTTP_RESPONSE const& resp, std::function<void (Seq, Seq)> f)
	{
		EnumKnownResponseHeaders([&] (Seq name, HTTP_HEADER_ID id) -> EnumInstr::E
			{
				HTTP_KNOWN_HEADER const& h = resp.Headers.KnownHeaders[id];
				if (h.pRawValue != nullptr && h.RawValueLength != 0)
					f(name, Seq(h.pRawValue, h.RawValueLength));
				return EnumInstr::Continue;
			} );

		for (USHORT i=0; i!=resp.Headers.UnknownHeaderCount; ++i)
		{
			HTTP_UNKNOWN_HEADER const& h = resp.Headers.pUnknownHeaders[i];
			EnsureThrow(h.pName != nullptr && h.NameLength != 0 && h.pRawValue != nullptr && h.RawValueLength != 0);
			f(Seq(h.pName, h.NameLength), Seq(h.pRawValue, h.RawValueLength));
		}
	}


	bool BhtpnServerThread::GetKnownRequestHeaderId(HTTP_HEADER_ID& id, Seq name)
	{
		bool found = false;
		EnumKnownRequestHeaders([&] (Seq enumName, HTTP_HEADER_ID enumId) -> EnumInstr::E
			{
				if (name.EqualInsensitive(enumName)) { id = enumId; found = true; return EnumInstr::Stop; }
				return EnumInstr::Continue;
			} );
		return found;
	}


	void BhtpnServerThread::SendPreparedResponse(HttpRequest& req, HTTP_RESPONSE* resp)
	{
		EnsureThrow(resp != nullptr);

		try
		{
			// Add Server header if there isn't one
			Seq server("Atomic-BhtpnServer");
			HTTP_KNOWN_HEADER& serverHdr = resp->Headers.KnownHeaders[HttpHeaderServer];
			if (serverHdr.pRawValue == nullptr || serverHdr.RawValueLength == 0)
			{
				serverHdr.pRawValue = (PCSTR) server.p;
				serverHdr.RawValueLength = NumCast<USHORT>(server.n);
			}

			// Add Date header if there isn't one
			{
				Str date;
				HTTP_KNOWN_HEADER& dateHdr = resp->Headers.KnownHeaders[HttpHeaderDate];
				if (dateHdr.pRawValue == nullptr || dateHdr.RawValueLength == 0)
				{
					date.Obj(req.RequestTime(), TimeFmt::Http);
					dateHdr.pRawValue = (PCSTR) date.Ptr();
					dateHdr.RawValueLength = NumCast<USHORT>(date.Len());
				}
			}

			{
				// Measure size of response parts before body
				sizet preBodySize = 0;
				preBodySize += EncodeVarUInt64_Size(resp->StatusCode);
				preBodySize += EncodeVarStr_Size(resp->ReasonLength);

				sizet nrHeaders = 0;
				EnumResponseHeaders(*resp, [&] (Seq name, Seq value)
					{
						preBodySize += EncodeVarStr_Size(name.n);
						preBodySize += EncodeVarStr_Size(value.n);
						++nrHeaders;
					} );
				preBodySize += EncodeVarUInt64_Size(nrHeaders);

				// Encode response parts before body
				Str        buf;
				Enc::Meter meter = buf.FixMeter(preBodySize);

				EncodeVarUInt64 (buf, resp->StatusCode);
				EncodeVarStr    (buf, Seq(resp->pReason, resp->ReasonLength));
				EncodeVarUInt64 (buf, nrHeaders);

				EnumResponseHeaders(*resp, [&] (Seq name, Seq value)
					{
						EncodeVarStr(buf, name);
						EncodeVarStr(buf, value);
					} );

				EnsureThrow(meter.Met());

				// Send response parts before body
				m_writer->SetExpireMs(IoTimeoutMs);
				m_writer->Write(buf);
			}

			// Send response body
			for (USHORT i=0; i!=resp->EntityChunkCount; ++i)
			{
				HTTP_DATA_CHUNK const& chunk = resp->pEntityChunks[i];
				switch (chunk.DataChunkType)
				{
				case HttpDataChunkFromMemory:
					if (chunk.FromMemory.BufferLength != 0)
						SendResponseBodyChunk(Seq(chunk.FromMemory.pBuffer, chunk.FromMemory.BufferLength));
					break;

				case HttpDataChunkFromFileHandle:
					SendChunkFromFile(chunk.FromFileHandle.FileHandle, chunk.FromFileHandle.ByteRange);
					break;

				default:
					EnsureThrow(!"Unsupported response data chunk type");
				}
			}

			// Send end of response body
			SendResponseBodyChunk(Seq());
		}
		catch (Writer::WriteWinErr const& e)
		{
			if (e.m_code == ERROR_NO_DATA)		// 232 - "The pipe is being closed."
				throw CommonSendErr();

			throw;
		}
	}


	void BhtpnServerThread::SendChunkFromFile(HANDLE fileHandle, HTTP_BYTE_RANGE const& byteRange)
	{
		EnsureThrow(fileHandle != 0 && fileHandle != INVALID_HANDLE_VALUE);

		LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = NumCast<LONGLONG>(byteRange.StartingOffset.QuadPart);
		if (!SetFilePointerEx(fileHandle, distanceToMove, 0, FILE_BEGIN))
			{ LastWinErr e; throw e.Make<>("SendChunkFromFile: SetFilePointerEx failed with error"); }

		HandleReader chunkReader(fileHandle);
		chunkReader.SetExpireMs(IoTimeoutMs);
		chunkReader.SetStopCtl(*this);

		uint64 bytesToGo = byteRange.Length.QuadPart;
		try
		{
			while (bytesToGo != 0)
			{
				chunkReader.Read(
					[&] (Seq& data) -> Reader::ReadInstr
					{
						if (data.n)
						{
							sizet subChunkSize = NumCast<sizet>(PickMin<uint64>(data.n, bytesToGo));
							Seq subChunk = data.ReadBytes(subChunkSize);
							SendResponseBodyChunk(subChunk);
							bytesToGo -= subChunk.n;
						}
						return Reader::ReadInstr::Done;
					} );
			}
		}
		catch (Reader::ReachedEnd const&)
		{
		}
	}


	void BhtpnServerThread::SendResponseBodyChunk(Seq chunk)
	{
		// Encode chunk size
		Str encodedSize;
		EncodeVarUInt64(encodedSize, chunk.n);

		// Send chunk size and data
		m_writer->SetExpireMs(IoTimeoutMs);
		m_writer->Write(encodedSize);
		if (chunk.n != 0)
			m_writer->Write(chunk);
	}

}
