#pragma once

#include "AtHttpStatus.h"
#include "AtMimeReadWrite.h"
#include "AtNameValuePairs.h"
#include "AtOpt.h"
#include "AtSocket.h"
#include "AtStr.h"
#include "AtTime.h"
#include "AtUtfWin.h"


namespace At
{
	// Wraps an HTTP_REQUEST pointer and provides additional methods for easier access

	struct WebServerType  { enum E { None, Http, Bhtpn }; };
	struct CookieSecure   { enum E { No, Yes }; };
	struct CookieHttpOnly { enum E { No, Yes }; };

	class HttpRequest : public NoCopy
	{
	public:
		// As of 2018-03-24, with help from https://stackoverflow.com/a/5383647/4889365 and
		// http://browsercookielimits.squawky.net/ - all tested on Windows 8.1:
		// - Chrome 65 allows 4096 bytes per cookie; Firefox 59 allows 4097; IE 11 allows 5117.
		// - Chrome 65 allows 180 cookies per domain; Firefox 59 allows 1000+; IE 11 allows 50.
		enum { MaxCookieBytes = 4096 };

		struct Error
		{
			Error(uint statusCode) : m_statusCode(statusCode) {}
			uint m_statusCode {};
		};

		struct Redirect
		{
			Redirect(uint statusCode, Seq uri) : m_statusCode(statusCode), m_uri(uri) {}
			uint m_statusCode {};
			Str m_uri;
		};

		HttpRequest(HTTP_REQUEST* req, WebServerType::E serverType) : m_req(req), m_serverType(serverType), m_multipartBody(Mime::Part::Root) {}
	
		void SetBody(Seq body) { m_body = body; }	// Body data must be stored externally for the lifetime of the request
	
		HTTP_REQUEST* operator-> () { return m_req; }
		HTTP_REQUEST const* operator-> () const { return m_req; }

		WebServerType::E ServerType() const { return m_serverType; }

		bool IsSecure() const { return m_req->pSslInfo != nullptr; }
		CookieSecure::E GetCookieSecure() const { return IsSecure() ? CookieSecure::Yes : CookieSecure::No; };
		CookieSecure::E GetCookieSecure_Strict() const;
		CookieHttpOnly::E GetCookieHttpOnly() const;
	
		bool IsGetOrHead () const { return m_req->Verb == HttpVerbGET || m_req->Verb == HttpVerbHEAD; }
		bool IsHead      () const { return m_req->Verb == HttpVerbHEAD; }
		bool IsPost      () const { return m_req->Verb == HttpVerbPOST; }
	
		sizet BodySize () const { return m_body.n; }
		Seq   Body     () const { return m_body; }
	
		Seq FullUrl      () const { if (!m_fullUrl.Any())      ConvertUtf16(m_req->CookedUrl.pFullUrl,     m_req->CookedUrl.FullUrlLength,     m_fullUrl    ); return m_fullUrl.Ref();     }
		Seq HostAndPort  () const { if (!m_hostAndPort.Any())  ConvertUtf16(m_req->CookedUrl.pHost,        m_req->CookedUrl.HostLength,        m_hostAndPort); return m_hostAndPort.Ref(); }
		Seq AbsPath      () const { if (!m_absPath.Any())      ConvertUtf16(m_req->CookedUrl.pAbsPath,     m_req->CookedUrl.AbsPathLength,     m_absPath    ); return m_absPath.Ref();     }
		Seq QueryString  () const { if (!m_queryString.Any())  ConvertUtf16(m_req->CookedUrl.pQueryString, m_req->CookedUrl.QueryStringLength, m_queryString); return m_queryString.Ref(); }

		Seq Host_NoPort  () const { return HostAndPort().ReadToByte(':'); }
		Str AbsPathAndQS () const;

		// Reproduces the same absolute path and query string, but replaces the specified query string parameter with the specified value.
		// The replaceName, replaceValue parameters should be unencoded (will be URL-encoded by the function).
		Str AbsPathAndQS_ReplaceParam(Seq replaceName, Seq replaceValue);

		Seq RawKnownHeader(HTTP_KNOWN_HEADER const& h) const	{ return Seq(h.pRawValue, h.RawValueLength); }
		Seq RawKnownHeader(HTTP_HEADER_ID id) const				{ return RawKnownHeader(m_req->Headers.KnownHeaders[id]); }
		Seq Referrer() const { if (!m_referrer.Any()) { m_referrer.Init(); ToUtf8Norm(RawKnownHeader(HttpHeaderReferer), m_referrer.Ref(), CP_UTF8); } return m_referrer.Ref(); }
	
		InsensitiveNameValuePairsWithStore const& CookiesNvp () const { if (!m_cookiesNvp.Any()) ParseCookiesNvp(); return m_cookiesNvp.Ref();      }
		InsensitiveNameValuePairsWithStore&       CfmNvp     ()       { if (!m_cfmNvp.Any())     m_cfmNvp.Init();   return m_cfmNvp.Ref();          }  // Initialized in WebRequestHandler::CheckCfmCookie()
		InsensitiveNameValuePairsWithStore const& CfmNvp     () const { if (!m_cfmNvp.Any())     m_cfmNvp.Init();   return m_cfmNvp.Ref();          }  // Initialized in WebRequestHandler::CheckCfmCookie()
		InsensitiveNameValuePairsWithStore const& QueryNvp   () const { if (!m_queryNvp.Any())   ParseQueryNvp();   return m_queryNvp.Ref();        }
		InsensitiveNameValuePairsWithStore const& PostNvp    () const { if (!m_postNvp.Any())    ParsePostNvp();    return m_postNvp.Ref();         }
		Vec<Mime::Part>                    const& BodyParts  () const { if (!m_postNvp.Any())    ParsePostNvp();    return m_multipartBody.m_parts; }

		Seq CookiesNvp (Seq name) const { return CookiesNvp ().Get(name); }
		Seq CfmNvp     (Seq name) const { return CfmNvp     ().Get(name); }		// Initialized in WebRequestHandler::CheckCfmCookie()
		Seq QueryNvp   (Seq name) const { return QueryNvp   ().Get(name); }
		Seq PostNvp    (Seq name) const { return PostNvp    ().Get(name); }

		void GetOptStrFromNvp(Opt<Str>& optStr, InsensitiveNameValuePairsWithStore const& nvp, Seq name);

		Vec<Seq>& ConsumedPostFormSecurityElements() const { return m_consumedPostFormSecurityElements; }

		void ForEachFileInput(std::function<bool (Mime::Part const& part, Seq fileName)> action);
		void ForEachLineInEachFileInput(std::function<bool (Mime::Part const& part, Seq fileName, Seq line)> action);

		PinStore& GetPinStore() { return m_pinStore; }

		uint64 RequestId() const { return m_req->RequestId; }
		Time RequestTime() const { if (!m_requestTime) m_requestTime = Time::StrictNow(); return m_requestTime; }

		struct BodyType { enum E { Uninitialized, Unknown, AppWwwForm, MultipartFormData }; };
		BodyType::E RequestBodyType() const { if (m_bodyType == BodyType::Uninitialized) ParseContentType(); return m_bodyType; }
		UINT RequestCp() const { if (!m_requestCp) ParseContentType(); return m_requestCp; }

		SockAddr const& RemoteAddr() const;
		SockAddr const& LocalAddr() const;

		Seq RemoteAddrOnly () const { if (!m_remoteAddrOnly.Any()) m_remoteAddrOnly.Obj(RemoteAddr(), SockAddr::AddrOnly); return m_remoteAddrOnly; }
		Seq RemoteIdAddr   () const { if (!m_remoteIdAddr.Any()) { SockAddr a; a.SetToIdAddressOf(RemoteAddr()); m_remoteIdAddr.Obj(a, SockAddr::AddrOnly); } return m_remoteIdAddr; }

	private:
		HTTP_REQUEST* m_req {};
		WebServerType::E m_serverType { WebServerType::None };
		Seq m_body;
		uint64 m_requestId {};

		mutable Opt<Str> m_fullUrl;
		mutable Opt<Str> m_hostAndPort;
		mutable Opt<Str> m_absPath;
		mutable Opt<Str> m_queryString;
		mutable Opt<Str> m_absPathAndQS;

		mutable Opt<Str> m_referrer;

		mutable Opt<InsensitiveNameValuePairsWithStore> m_cookiesNvp;
		mutable Opt<InsensitiveNameValuePairsWithStore> m_cfmNvp;
		mutable Opt<InsensitiveNameValuePairsWithStore> m_queryNvp;
		mutable Opt<InsensitiveNameValuePairsWithStore> m_postNvp;
		mutable Vec<Seq> m_consumedPostFormSecurityElements;
		mutable PinStore m_pinStore { 4000 };
		mutable Mime::Part m_multipartBody;

		mutable Time m_requestTime;

		mutable BodyType::E m_bodyType { BodyType::Uninitialized };
		mutable UINT m_requestCp {};

		mutable SockAddr m_remoteSa;
		mutable SockAddr m_localSa;

		mutable Str m_remoteAddrOnly;
		mutable Str m_remoteIdAddr;
	
		void ConvertUtf16(PCWSTR pStr, USHORT nBytes, Opt<Str>& out) const;
		void ParseContentType() const;
		void ParseCookiesNvp() const;
		void ParseQueryNvp() const;
		void ParsePostNvp() const;
	};
}
