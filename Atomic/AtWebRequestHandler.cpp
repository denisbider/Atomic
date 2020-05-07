#include "AtIncludes.h"
#include "AtWebRequestHandler.h"

#include "AtBaseXY.h"
#include "AtNumCvt.h"
#include "AtTime.h"
#include "AtToken.h"
#include "AtWinStr.h"


namespace At
{

	SymCrypt WebRequestHandler::s_symCrypt;


	void WebRequestHandler::StaticInit()
	{
		s_symCrypt.Generate();
	}


	WebRequestHandler::WebRequestHandler() : m_pinStore(4000)
	{
		ZeroMemory(&m_response, sizeof(HTTP_RESPONSE));
	}


	WebRequestHandler::~WebRequestHandler()
	{
		for (HANDLE& h : m_responseHandles)
			if (h != 0 && h != INVALID_HANDLE_VALUE)
			{
				CloseHandle(h);
				h = INVALID_HANDLE_VALUE;
			}
	
		m_responseHandles.Clear();
	}


	void WebRequestHandler::UpdateResponseEntityChunksPtr()
	{
		m_response.EntityChunkCount = NumCast<USHORT>(m_responseBodyChunks.Len());
		m_response.pEntityChunks = &m_responseBodyChunks[0];
	}


	void WebRequestHandler::AddResponseBodyChunk_NoCopy(Seq str)
	{
		HTTP_DATA_CHUNK& chunk = m_responseBodyChunks.Add();
		chunk.DataChunkType = HttpDataChunkFromMemory;
		chunk.FromMemory.BufferLength = NumCast<ULONG>(str.n);
		chunk.FromMemory.pBuffer = (PVOID) str.p;

		UpdateResponseEntityChunksPtr();	
	}


	void WebRequestHandler::AddResponseBodyChunk(HANDLE hFile, uint64 startingOffset, uint64 length)
	{
		HTTP_DATA_CHUNK& chunk = m_responseBodyChunks.Add();
		chunk.DataChunkType = HttpDataChunkFromFileHandle;
		chunk.FromFileHandle.ByteRange.StartingOffset.QuadPart = startingOffset;
		chunk.FromFileHandle.ByteRange.Length.QuadPart = length;
		chunk.FromFileHandle.FileHandle = hFile;
	
		UpdateResponseEntityChunksPtr();
	}


	namespace { Str CfmCookieName(Seq token) { return Str().ReserveExact(3 + token.n).Add("cfm").Add(token); } }


	Str WebRequestHandler::AddCfmCookie(HttpRequest& req, Seq name, Seq value)
	{
		InsensitiveNameValuePairs nvp;
		nvp.Add(name, value);
		return AddCfmCookie(req, nvp);
	}

	Str WebRequestHandler::AddCfmCookie(HttpRequest& req, Seq name1, Seq value1, Seq name2, Seq value2)
	{
		InsensitiveNameValuePairs nvp;
		nvp.Add(name1, value1);
		nvp.Add(name2, value2);
		return AddCfmCookie(req, nvp);
	}

	Str WebRequestHandler::AddCfmCookie(HttpRequest& req, Seq name1, Seq value1, Seq name2, Seq value2, Seq name3, Seq value3)
	{
		InsensitiveNameValuePairs nvp;
		nvp.Add(name1, value1);
		nvp.Add(name2, value2);
		nvp.Add(name3, value3);
		return AddCfmCookie(req, nvp);
	}

	Str WebRequestHandler::AddCfmCookieWithContext(HttpRequest& req, Seq context, InsensitiveNameValuePairsBase const& nvp)
	{
		// Calculate encoded size of cookie plaintext
		uint64 ftReqTime   { req.RequestTime().ToFt() };
		sizet  encodedSize { EncodeVarUInt64_Size(ftReqTime) + EncodeVarStr_Size(context.n) };
		sizet  nrPairs     {};

		for (NameValuePair const& nv : nvp)
		{
			++nrPairs;
			encodedSize += EncodeVarStr_Size(nv.m_name.n) + EncodeVarStr_Size(nv.m_value.n);
		}

		encodedSize += EncodeVarUInt64_Size(nrPairs);

		// Encode cookie plaintext
		Str plaintext;
		Enc::Meter meter = plaintext.FixMeter(encodedSize);

		EncodeVarUInt64 (plaintext, ftReqTime);
		EncodeVarStr    (plaintext, context);
		EncodeVarUInt64 (plaintext, nrPairs);

		for (NameValuePair const& nv : nvp)
		{
			EncodeVarStr(plaintext, nv.m_name);
			EncodeVarStr(plaintext, nv.m_value);
		}

		EnsureThrow(meter.Met());

		// Cookie name is prefixed so that this mechanism cannot be used to remove an arbitrary well-known cookie, e.g. to logout a user
		Str token         { Token::Generate() };
		Str cfmCookieName { CfmCookieName(token) };
		AddEncryptedSessionCookie(req, cfmCookieName, plaintext, "", "/");
		return token;
	}


	bool WebRequestHandler::CheckCfmCookie(HttpRequest& req)
	{
		Seq  context = typeid(*this).name();
		Seq  token   = req.QueryNvp("cfm");
		bool success {};
		if (token.n == Token::Len)
		{
			Str cfmCookieName { CfmCookieName(token) };
			Seq ciphertext    { req.CookiesNvp(cfmCookieName) };
			if (ciphertext.n)
			{
				Str plaintext;		
				if (DecryptCookie(ciphertext, plaintext))
				{
					Seq    reader        { plaintext };
					uint64 ftPrevReqTime {};
					if (DecodeVarUInt64(reader, ftPrevReqTime))
					{
						Time prevReqTime { Time::FromFt(ftPrevReqTime) };
						if ((req.RequestTime() - prevReqTime).ToSeconds() <= MaxCfmCookieAgeSeconds)
						{
							Seq decodedContext;
							if (DecodeVarStr(reader, decodedContext, MaxCfmContextBytes) &&
								decodedContext.EqualExact(context))
							{
								uint64 nrPairs {};
								if (DecodeVarUInt64(reader, nrPairs))
								{
									req.CfmNvp().Clear();
									success = true;

									for (sizet i=0; i!=nrPairs; ++i)
									{
										Seq name, value;
										if (!DecodeVarStr(reader, name,  MaxCfmNameBytes  ) ||
											!DecodeVarStr(reader, value, MaxCfmValueBytes ))
											{ success = false; break; }

										req.CfmNvp().Add(name, value);
									}
								}
							}
						}
					}
				}

				if (!req.IsHead())
				{
					// Confirmation tokens are plainly visible in address bar, and pages we link to could see them via referer info.
					// Therefore, remove their corresponding cookie as soon as they're used
					RemoveCookie(cfmCookieName, "", "/");
				}
			}
		}

		return success;
	}


	void WebRequestHandler::AddCookie(Seq name, Seq value, Seq domain, Seq path, CookieSecure::E secure, CookieHttpOnly::E httpOnly, int expiresSeconds)
	{
		// Value must consist of *cookie-octet as per RFC 6265, section 4.1.1
		auto isCookieOctet = [] (uint c) -> bool { return c>=0x21 && c<=0x7E && !ZChr("\",;\\", c); };
		EnsureThrow(!value.ContainsAnyByteNotOfType(isCookieOctet));

		Str str;
		str.ReserveExact(
			name.n + 1 + value.n		// name=value
			+ 40					    // ; Expires=DATE
			+ 7 + path.n				// ; Path=PATH
			+ 9 + domain.n				// ; Domain=DOMAIN
			+ 8							// ; Secure	
			+ 10);						// ; HttpOnly	
	
		str.Add(name).Ch('=').Add(value);

		if (expiresSeconds < 0)
			str.Add("; Expires=").Obj(Time::FromUnixTime(0), TimeFmt::Http);
		else if (expiresSeconds != 0)
		{
			Time ftExpires = Time::StrictNow() + Time::FromSeconds((uint64) expiresSeconds);
			str.Add("; Expires=").Obj(ftExpires, TimeFmt::Http);
		}
	
		if (path.n)                          str.Add("; Path=").Add(path);
		if (domain.n)                        str.Add("; Domain=").Add(domain);
		if (secure   != CookieSecure::No)    str.Add("; Secure");
		if (httpOnly != CookieHttpOnly::No)  str.Add("; HttpOnly");
	
		SetResponseHeader_SetCookie(str);
	}


	void WebRequestHandler::AddEncryptedCookie(Seq name, Seq value, Seq domain, Seq path, CookieSecure::E secure, CookieHttpOnly::E httpOnly, int expiresSeconds)
	{
		Str encrypted;
		s_symCrypt.Encrypt(value, encrypted);
	
		Str encoded;
		Base64::UrlFriendlyEncode(encrypted, encoded, Base64::Padding::No);
	
		AddCookie(name, encoded, domain, path, secure, httpOnly, expiresSeconds);
	}


	bool WebRequestHandler::DecryptCookie(Seq encoded, Enc& value)
	{
		Str encrypted;
		Base64::UrlFriendlyDecode(encoded, encrypted);	
		return s_symCrypt.Decrypt(encrypted, value);
	}


	void WebRequestHandler::SetKnownResponseHeader(HTTP_HEADER_ID hdrId, Seq s)
	{
		Seq added = AddResponseStr(s);

		HTTP_KNOWN_HEADER& hdr = m_response.Headers.KnownHeaders[hdrId];
		hdr.pRawValue = (PCSTR) added.p;
		hdr.RawValueLength = NumCast<USHORT>(added.n);
	}


	void WebRequestHandler::SetUnknownResponseHeader(Seq name, Seq value)
	{
		Seq nameAdded = AddResponseStr(name);
		Seq valueAdded = AddResponseStr(value);

		HTTP_UNKNOWN_HEADER& hdr = m_responseUnknownHeaders.Add();
		hdr.NameLength = NumCast<USHORT>(nameAdded.n);
		hdr.pName = (PCSTR) nameAdded.p;
		hdr.RawValueLength = NumCast<USHORT>(valueAdded.n);
		hdr.pRawValue = (PCSTR) valueAdded.p;
	
		m_response.Headers.UnknownHeaderCount = NumCast<USHORT>(m_responseUnknownHeaders.Len());
		m_response.Headers.pUnknownHeaders = &m_responseUnknownHeaders[0];
	}


	void WebRequestHandler::SetResponseContentType(Seq s)
	{
		SetResponseHeader_ContentType(s);
		SetResponseHeader_XContentTypeOptions("nosniff");
		if (s.StartsWithExact("text/"))
		{
			// As of 2017-07-23, X-Frame-Options appears to be the most widely effective measure to prevent clickjacking.
			// Clickjacking is when a visible but click-transparent malicious page is rendered over an invisible but click-receiving legitimate page.
			// The user clicks a UI element on the visible malicious page, but the click is instead received and acted upon by the hidden legitimate page.
			// X-Frame-Options is being deprecated in favor of CSP2 frame-ancestors, but currently, no version of Edge or IE implements frame-ancestors.
			// The SAMEORIGIN value appears to be a reasonable default for all pages that should probably just be a default for all pages in browsers.
			// However, for some reason, in the absence of an explicit directive, browsers render pages vulnerable to clickjacking by default.
			SetResponseHeader_XFrameOptions("SAMEORIGIN");
		}
	}


	void WebRequestHandler::SetRedirectResponse(uint statusCode, Seq uri)
	{
		m_response.StatusCode = (USHORT) statusCode;
		SetResponseHeader_Location(uri);
	}


	void WebRequestHandler::SetFileResponse(Seq fullPath, Seq contentType)
	{
		// This implementation is currently highly inefficient if the server is being bombarded with requests.
		// Needs to be replaced with a caching implementation. Also needs to not throw, and instead fail efficiently, if the file is not found.

		HANDLE hFile = CreateFileW(WinStr(fullPath).Z(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			DWORD rc = GetLastError();
			if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)
				throw HttpRequest::Error(HttpStatus::NotFound);

			throw WinErr<>(rc, Str(__FUNCTION__ ": CreateFile for ").Add(fullPath));
		}
		else
		{
			AddResponseHandle(hFile);
			AddResponseBodyChunk(hFile);

			m_response.StatusCode = HttpStatus::OK;
			SetResponseContentType(contentType);
		}
	}

}
