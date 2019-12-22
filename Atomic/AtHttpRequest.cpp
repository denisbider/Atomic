#include "AtIncludes.h"
#include "AtHttpRequest.h"

#include "AtCharsets.h"
#include "AtException.h"
#include "AtMimeReadWrite.h"
#include "AtNumCvt.h"
#include "AtPostFormSecurity.h"
#include "AtWinErr.h"
#include "AtUrlEncode.h"
#include "AtUtfWin.h"


namespace At
{

	CookieSecure::E HttpRequest::GetCookieSecure_Strict() const
	{
		CookieSecure::E cs = GetCookieSecure();
		if (cs != CookieSecure::Yes)
			if (ServerType() != WebServerType::Bhtpn)
			{
				EnsureThrow(LocalAddr().IsLocal());
				EnsureThrow(RemoteAddr().IsLocal());
			}

		return cs;
	}

	
	CookieHttpOnly::E HttpRequest::GetCookieHttpOnly() const
	{
		if (ServerType() == WebServerType::Bhtpn)
			return CookieHttpOnly::No;

		return CookieHttpOnly::Yes;
	}


	void HttpRequest::ConvertUtf16(PCWSTR pStr, USHORT nBytes, Opt<Str>& out) const
	{
		if ((nBytes % 2) != 0)
			throw ZLitErr("HttpRequest: Unexpected odd number of bytes in UTF-16 input");

		out.Init();
		OnExit outClear( [&] () { out.Clear(); } );

		Vec<wchar_t> convertBuf;
		NormalizeUtf16(pStr, nBytes/2, convertBuf);
		FromUtf16(convertBuf.Ptr(), NumCast<int>(convertBuf.Len()), out.Ref(), CP_UTF8);

		outClear.Dismiss();
	}


	Str HttpRequest::AbsPathAndQS() const
	{
		if (!m_absPathAndQS.Any())
		{
			Seq absPath = AbsPath();
			Seq qs = QueryString();

			m_absPathAndQS.Init();
			if (!qs.Any())
				m_absPathAndQS->Set(absPath);
			else
				m_absPathAndQS->SetAdd(absPath, "?", qs);
		}

		return m_absPathAndQS.Ref();
	}


	Str HttpRequest::AbsPathAndQS_ReplaceParam(Seq replaceName, Seq replaceValue)
	{
		Str r = AbsPath();
		char paramSep = '?';

		auto addParam = [&r, &paramSep] (Seq name, Seq value)
			{
				r.Ch(paramSep).UrlEncode(name).Ch('=').UrlEncode(value);
				paramSep = '&';
			};

		for (NameValuePair const& nv : QueryNvp())
			if (!nv.m_name.EqualInsensitive(replaceName))
				addParam(nv.m_name, nv.m_value);

		addParam(replaceName, replaceValue);
		return r;
	}


	void HttpRequest::GetOptStrFromNvp(Opt<Str>& optStr, InsensitiveNameValuePairsWithStore const& nvp, Seq name)
	{
		// Method to be kept in sync with HtmlBuilder::{InputOptStr, TextAreaOpt}.
		Str namePresent = Str::Join(name, "_present");
		if (nvp.Get(namePresent) == "1")
			optStr.ReInit().Set(nvp.Get(name));
		else
			optStr.Clear();
	}


	void HttpRequest::ForEachFileInput(std::function<bool (Mime::Part const& part, Seq fileName)> action)
	{
		for (Mime::Part const& part : BodyParts())
		{
			if (part.m_contentDisp.Any() &&
			    part.m_contentDisp->m_type.EqualInsensitive("form-data"))
			{
				Seq fileName { part.m_contentDisp->m_params.Get("filename") };
				if (fileName.Any())
					if (!action(part, fileName))
						break;
			}
		}
	}


	void HttpRequest::ForEachLineInEachFileInput(std::function<bool (Mime::Part const& part, Seq fileName, Seq line)> action)
	{
		ForEachFileInput( [&] (Mime::Part const& part, Seq fileName) -> bool
			{
				Mime::PartContent content { part, m_pinStore };
				if (content.m_success)
				{
					Seq reader { content.m_decoded };
					while (reader.n)
					{
						Seq line { reader.ReadToFirstByteOf("\r\n").Trim() };
						reader.DropToFirstByteNotOf("\r\n \t");
						if (line.n)
						{
							if (!action(part, fileName, line))
								return false;
						}
					}
				}

				return true;
			} );
	}


	SockAddr const& HttpRequest::RemoteAddr() const
	{
		if (!m_remoteSa.Valid())
		{
			if (!m_remoteSa.Set(m_req->Address.pRemoteAddress, SockAddr::Auto).Valid())
				throw ZLitErr("HttpRequest: Error setting remote address");
		}

		return m_remoteSa;
	}


	SockAddr const& HttpRequest::LocalAddr() const
	{
		if (!m_localSa.Valid())
		{
			if (!m_localSa.Set(m_req->Address.pLocalAddress, SockAddr::Auto).Valid())
				throw ZLitErr("HttpRequest: Error setting local address");
		}

		return m_localSa;
	}


	void HttpRequest::ParseContentType() const
	{
		// Determine charset and body type.
		// For charset, fail if provided but unsupported. If not provided, assume UTF-8
		BodyType::E bt = BodyType::Unknown;
		UINT cp = CP_UTF8;

		Seq ctStr { RawKnownHeader(HttpHeaderContentType) };
		if (ctStr.n)
		{
			ParseTree pt { ctStr };
			if (pt.Parse(Mime::C_content_type_inner))
			{
				PinStore store { 2 * ctStr.n };
				Mime::ContentType ct;
				ct.Read(pt.Root(), store);
				if (ct.IsAppWwwFormUrlEncoded())
					bt = BodyType::AppWwwForm;
				else if (ct.IsMultipartFormData())
					bt = BodyType::MultipartFormData;

				Seq charset { ct.m_params.Get("charset") };
				if (charset.n)
				{
					cp = CharsetNameToWindowsCodePage(charset);
					if (!cp)
						throw Error(HttpStatus::BadRequest);
				}
			}
		}

		m_bodyType = bt;
		m_requestCp = cp;
	}


	void HttpRequest::ParseCookiesNvp() const
	{
		if (!m_cookiesNvp.Any())
		{
			m_cookiesNvp.Init();

			HTTP_KNOWN_HEADER& hdr = m_req->Headers.KnownHeaders[HttpHeaderCookie];
			Seq cookies(hdr.pRawValue, hdr.RawValueLength);
	
			while (true)
			{
				cookies.DropToFirstByteNotOf("; \t\r\n");
				if (!cookies.n)
					break;

				Seq name = cookies.ReadToFirstByteOf(";=");
				Seq value;
			
				if (cookies.n && cookies.p[0] != ';')
				{
					cookies.DropByte();
					value = cookies.ReadToByte(';');
				}

				// Ignore any cookies with non-ASCII-printable characters in name or value.
				// A well-behaved server should not be setting such cookies, and if the server did not set them,
				// a client certainly should not be sending them.

				if (!name .ContainsAnyByteNotOfType(Ascii::IsPrintable) &&
					!value.ContainsAnyByteNotOfType(Ascii::IsPrintable))
				{
					m_cookiesNvp->Add(name.Trim(), value.Trim());
				}
			}
		}
	}


	void HttpRequest::ParseQueryNvp() const
	{
		if (!m_queryNvp.Any())
		{
			m_queryNvp.Init();
			ParseUrlEncodedNvp(m_queryNvp.Ref(), QueryString(), RequestCp());
		}
	}


	void HttpRequest::ParsePostNvp() const
	{
		if (!m_postNvp.Any())
		{
			InsensitiveNameValuePairsWithStore& postNvp { m_postNvp.Init() };

			if (Referrer().ContainsString("/unsafe/", CaseMatch::Insensitive))
				throw Error(HttpStatus::BadRequest);

			if (!PostFormSecurity::GetPostFormSecurityCookie(*this).Any())
				throw Error(HttpStatus::BadRequest);
			
			UINT cp = RequestCp();

			if (RequestBodyType() == BodyType::AppWwwForm)
			{
				ParseUrlEncodedNvp(postNvp, m_body, cp);
			}
			else if (RequestBodyType() == BodyType::MultipartFormData)
			{
				if (m_pinStore.BytesPerStr() > m_body.n)
					m_pinStore.SetBytesPerStr(PickMax<sizet>(100, m_body.n));

				Seq bodyReader { m_body };
				Mime::PartReadCx prcx;
				prcx.m_decodeDepth = 1;
				if (!m_multipartBody.Read(bodyReader, m_pinStore, prcx))
					throw Error(HttpStatus::BadRequest);

				Vec<wchar_t> convertBuf1, convertBuf2;
				Str nameUtf8, valueUtf8;

				for (Mime::Part const& part : m_multipartBody.m_parts)
					if (part.m_contentDisp.Any() &&
						part.m_contentDisp->m_type.EqualInsensitive("form-data"))
					{
						// Add only form-data parts that aren't files to m_postNvp
						Seq filename { part.m_contentDisp->m_params.Get("filename") };
						if (!filename.Any() &&
							(!part.m_contentType.Any() || part.m_contentType->IsTextPlain()))
						{
							Seq name { part.m_contentDisp->m_params.Get("name") };
							Mime::PartContent content { part, m_pinStore };

							ToUtf8Norm(name,              nameUtf8,  cp, convertBuf1, convertBuf2);
							ToUtf8Norm(content.m_decoded, valueUtf8, cp, convertBuf1, convertBuf2);

							postNvp.Add(nameUtf8, valueUtf8);
						}
					}
			}
			else
				throw Error(HttpStatus::UnsupportedMediaType);

			if (!PostFormSecurity::ConsumePostFormSecurityField(*this))		
				throw Error(HttpStatus::BadRequest);
		}
	}

}
