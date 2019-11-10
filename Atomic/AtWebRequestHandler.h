#pragma once

#include "AtCrypt.h"
#include "AtEntityStore.h"
#include "AtException.h"
#include "AtHttpRequest.h"
#include "AtPinStore.h"
#include "AtRp.h"
#include "AtStr.h"
#include "AtVec.h"


namespace At
{
	enum class ReqResult { Invalid, Done, Continue };

	class WebRequestHandler : public RefCountable
	{
	private:
		static SymCrypt s_symCrypt;

	public:
		// Initializes s_symCrypt static object.
		// Must be called after the application has initialized At::Crypt.
		static void StaticInit();

		WebRequestHandler();	
		virtual ~WebRequestHandler();

		bool                      m_abortTxOnSuccess {};
		HTTP_RESPONSE             m_response;
		PinStore                  m_pinStore;
		Vec<HANDLE>               m_responseHandles;
		Vec<HTTP_DATA_CHUNK>      m_responseBodyChunks;
		Vec<HTTP_UNKNOWN_HEADER>  m_responseUnknownHeaders;

		void SetAbortTxOnSuccess() { m_abortTxOnSuccess = true; }
		bool AbortTxOnSuccess() const { return m_abortTxOnSuccess; }
	
		Seq  AddResponseStr              (Seq s)    { return m_pinStore.AddStr(s); }
		void AddResponseHandle           (HANDLE h) { m_responseHandles.Add(h); }
		void AddResponseBodyChunk        (Seq s)    { Seq added = AddResponseStr(s); AddResponseBodyChunk_NoCopy(added); }
		void AddResponseBodyChunk_NoCopy (Seq s);

		// DO NOT CLOSE file handle after adding it. It will be closed when WebRequestHandler is destroyed.
		void AddResponseBodyChunk(HANDLE hFile, uint64 startingOffset = 0, uint64 length = HTTP_BYTE_RANGE_TO_EOF);

		// The result of AddCfmCookie is a confirmation cookie token, and should be used as value of a "&cfm=" parameter in a URL.
		// CheckCfmCookie returns true if a valid confirmation cookie is found corresponding to a "&cfm=" query parameter.
		// If CheckCfmCookie returns true, it will initialize CfmNvp in HttpRequest.
		enum { MaxCfmCookieAgeSeconds = 300, MaxCfmContextBytes = 1000, MaxCfmPairs = 100, MaxCfmNameBytes = 100, MaxCfmValueBytes = 1000 };
		Str  AddCfmCookie   (HttpRequest& req, Seq context) { return AddCfmCookie(req, context, InsensitiveNameValuePairs()); }
		Str  AddCfmCookie   (HttpRequest& req, Seq context, Seq name,  Seq value);
		Str  AddCfmCookie   (HttpRequest& req, Seq context, Seq name1, Seq value1, Seq name2, Seq value2);
		Str  AddCfmCookie   (HttpRequest& req, Seq context, Seq name1, Seq value1, Seq name2, Seq value2, Seq name3, Seq value3);
		Str  AddCfmCookie   (HttpRequest& req, Seq context, InsensitiveNameValuePairsBase const& nvp);
		bool CheckCfmCookie (HttpRequest& req, Seq context);

		void AddSessionCookie          (HttpRequest& req, Seq n, Seq v, Seq d, Seq p)              { AddCookie(n, v, d, p, req.GetCookieSecure_Strict(), req.GetCookieHttpOnly(), 0       ); }
		void AddSavedCookie            (HttpRequest& req, Seq n, Seq v, Seq d, Seq p, int expSecs) { AddCookie(n, v, d, p, req.GetCookieSecure_Strict(), req.GetCookieHttpOnly(), expSecs ); }
	
		void AddEncryptedSessionCookie (HttpRequest& req, Seq n, Seq v, Seq d, Seq p)              { AddEncryptedCookie(n, v, d, p, req.GetCookieSecure_Strict(), req.GetCookieHttpOnly(), 0       ); }
		void AddEncryptedSavedCookie   (HttpRequest& req, Seq n, Seq v, Seq d, Seq p, int expSecs) { AddEncryptedCookie(n, v, d, p, req.GetCookieSecure_Strict(), req.GetCookieHttpOnly(), expSecs ); }
	
		bool DecryptCookie(Seq encoded, Enc& value);	// Decrypt a cookie value set with AddEncrypted***Cookie
	
		void RemoveCookie(Seq name, Seq domain, Seq path)
		{
			AddCookie(name, "", domain, path, CookieSecure::Yes, CookieHttpOnly::Yes, -1);
			AddCookie(name, "", domain, path, CookieSecure::No,  CookieHttpOnly::Yes, -1);
			AddCookie(name, "", domain, path, CookieSecure::Yes, CookieHttpOnly::No,  -1);
			AddCookie(name, "", domain, path, CookieSecure::No,  CookieHttpOnly::No,  -1);
		}

		void SetResponseContentType(Seq s);
	
		void SetRedirectResponse (uint statusCode, Seq uri);
		void SetFileResponse     (Seq fullPath, Seq contentType);
	
	public:
		// Should process the HTTP request. Can optionally generate the response to send in m_response, and return ReqResult::Done.
		// If no response is generated, method should return ReqResult::Continue, and then GenResponse will be called outside of transaction.
		// Any strings that are referenced from within m_response must be added to m_pinStore.
		// This method is called from within a datastore transaction created by the request handling thread.
		// If the method completes successfully, the request handling thread will attempt to commit the transaction.
		// If the transaction was committed, the request handling thread will either:
		// - if return result is ReqResult::Done, send the generated response
		// - if return result is ReqResult::Continue, call GenResponse() to generate the response, and then send it
		// If the commit attempt failed with the RetryTransaction exception, the Process() method will be called again.
		// If the method causes any side effects outside of the datastore, it MUST be resilient to being called again.
		// If the method throws an exception deriving from std::exception, an "internal server error" response will be sent.
		virtual ReqResult Process(EntityStore& store, HttpRequest& req) = 0;

		// Called if Process() returned with ReqResult::Continue. Called outside of datastore transaction.
		virtual void GenResponse(HttpRequest& req) = 0;

	private:
		void UpdateResponseEntityChunksPtr();

		void AddCookie          (Seq name, Seq value, Seq domain, Seq path, CookieSecure::E secure, CookieHttpOnly::E httpOnly, int expiresSeconds);
		void AddEncryptedCookie (Seq name, Seq value, Seq domain, Seq path, CookieSecure::E secure, CookieHttpOnly::E httpOnly, int expiresSeconds);

		void SetKnownResponseHeader   (HTTP_HEADER_ID hdrId, Seq s);
		void SetUnknownResponseHeader (Seq name, Seq value);

		void SetResponseHeader_ContentType             (Seq s) { SetKnownResponseHeader  (HttpHeaderContentType,       s); }
		void SetResponseHeader_Location                (Seq s) { SetKnownResponseHeader  (HttpHeaderLocation,          s); }

		void SetResponseHeader_SetCookie               (Seq s) { SetUnknownResponseHeader("Set-Cookie",                s); }
		void SetResponseHeader_XContentTypeOptions     (Seq s) { SetUnknownResponseHeader("X-Content-Type-Options",    s); }
		void SetResponseHeader_XFrameOptions           (Seq s) { SetUnknownResponseHeader("X-Frame-Options",           s); }

	public:
		void SetResponseHeader_ContentDisposition      (Seq s) { SetUnknownResponseHeader("Content-Disposition",       s); }
		void SetResponseHeader_ContentSecurityPolicy   (Seq s) { SetUnknownResponseHeader("Content-Security-Policy",   s); }
		void SetResponseHeader_Refresh                 (Seq s) { SetUnknownResponseHeader("Refresh",                   s); }
		void SetResponseHeader_StrictTransportSecurity (Seq s) { SetUnknownResponseHeader("Strict-Transport-Security", s); }

		void SetResponseHeader_CacheControl            (Seq s) { SetKnownResponseHeader  (HttpHeaderCacheControl,      s); }
	};
}
