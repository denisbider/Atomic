#include "AtIncludes.h"
#include "AtPostFormSecurity.h"

#include "AtCrypt.h"
#include "AtMap.h"


namespace At
{

	Seq PostFormSecurity::PostFormSecurityField()
	{
		if (!m_havePostFormSecurity)
		{
			// Cross-site request forgery protection for POST forms:
			//
			// POST request carries a cookie value as follows: cookieElement := cookieToken bothTokensHash
			// POST form carries a security field as follows:  securityField := fieldToken  bothTokensHash
			//
			// cookieToken and fieldToken are generated using Token::Generate().
			// bothTokensHash is set to Token::Tokenize(SHA-256(cookieToken + fieldToken)).
			//
			// On receipt of POST form, receiver verifies:
			// - The bothTokensHash portion of both the cookie value and security field is identical.
			// - bothTokensHash matches Token::Tokenize(SHA-256(cookieToken + fieldToken)).
			//
			// This proves that whoever constructed the HTML form had the ability to set the corresponding cookie at the same time.
			// A less secure version is to just set cookie and field to same value, but that does not provide equally strong guarantees.

			Str cookieToken = Token::Generate();
			Str fieldToken = Token::Generate();

			Str bothTokens = Str::Join(cookieToken, fieldToken);
			Str bothTokensHash = Token::Tokenize(Hash::HashOf(bothTokens, CALG_SHA_256));
			m_postFormCookieElement.SetAdd(cookieToken, bothTokensHash);
			m_postFormSecurityField.SetAdd(fieldToken,  bothTokensHash);
			m_havePostFormSecurity = true;
		}

		return m_postFormSecurityField;
	}


	void PostFormSecurity::SetPostFormSecurityCookie(HttpRequest& req, WebRequestHandler& handler)
	{
		// Complication: User can simultaneously open windows with multiple forms.
		//
		// - Solution with usability problem: Allow POST form cookies to overwrite each other. Only the form that was opened last will work.
		//
		// - Solution with security problem: Allow use of same cookie by multiple forms. May permit cross-site request forgery if user recently viewed a form.
		//
		// - Strong solution: Maintain a big cookie that lists all outstanding POST form cookie values and removes those that are oldest or already used.
		//   Big cookie rather than multiple cookies because all browsers allow at least 4096 bytes per cookie, but many allow only 50 cookies per host.
		//   The browser will delete arbitrary cookies if cookie count is exceeded, including cookies not related to POST forms.
		//
		// We use the big cookie solution, with structure as follows:
		//
		//   big-cookie := protector *(":" protector)
		//   protector := unixTime64Hex cookieElement
		//
		// ... where cookieElement is as defined and generated in PostFormSecurityField().

		Time now = Time::StrictNow();
		Str newProtector;				// Leave empty if none to add

		if (m_havePostFormSecurity)
		{
			// It is more appropriate to order protectors by time of response rather than request.
			// This method will be called at time of response, so use the current time.
			uint64 nowUnixUnsigned = (uint64) now.ToUnixTime();
			newProtector.ReserveExact(ProtectorLen)
				.UInt(nowUnixUnsigned, 16, 16).Add(m_postFormCookieElement);
		}

		Seq existingCookie = req.CookiesNvp("PostForm");
		Str newCookieStr;
		Seq newCookie;

		if (!existingCookie.n)
			newCookie = newProtector;
		else
		{
			// Remove consumed and expired protectors
			Map<ProtectorInfo> protectors;
			ForEachProtectorInCookie(existingCookie, [&] (ProtectorInfo const& pi) -> bool
				{
					if (!pi.IsExpired(now))
						if (!req.ConsumedPostFormSecurityElements().Contains(pi.m_element))
							protectors.Add(pi);

					return true;
				} );

			// Drop oldest protectors until we can fit within maximum cookie size
			sizet estimatedSize;
			while (true)
			{
				estimatedSize = (1 + protectors.Len()) * (1 + ProtectorLen);
				if (estimatedSize < HttpRequest::MaxCookieBytes)
					break;

				protectors.Erase(protectors.begin());
			}

			// Encode the new cookie including the new protector
			newCookieStr.ReserveExact(estimatedSize).Set(newProtector);
			for (ProtectorInfo const& protector : protectors)
				newCookieStr.ChIfAny(':').Add(protector.m_protector);

			newCookie = newCookieStr;
		}

		if (newCookie.n)
			handler.AddSessionCookie(req, "PostForm", newCookie, "", "/");
	}


	bool PostFormSecurity::ConsumePostFormSecurityField(HttpRequest const& req)
	{
		// Decode POST form security field
		Seq field = req.PostNvp("PostForm");
		if (field.n != SecurityElementLen)
			return false;

		Seq fieldReader = field;
		Seq fieldToken = fieldReader.ReadBytes(Token::Len);
		Seq bothTokensHash = fieldReader;

		// Find candidate protector in POST form security cookie
		Seq cookie = GetPostFormSecurityCookie(req);
		ProtectorInfo piCandidate;

		ForEachProtectorInCookie(cookie, [&] (ProtectorInfo const& pi) -> bool
			{
				if (pi.m_bothTokensHash.EqualExact(bothTokensHash))
					{ piCandidate = pi; return false; }
				return true;
			} );

		if (!piCandidate.Any())
			return false;

		// Has the protector already been verified and consumed in this request?
		if (req.ConsumedPostFormSecurityElements().Contains(piCandidate.m_element))
			return true;

		// Verify protector
		Str bothTokens = Str::Join(piCandidate.m_cookieToken, fieldToken);
		Str calcHash = Token::Tokenize(Hash::HashOf(bothTokens, CALG_SHA_256));
		if (!bothTokensHash.EqualExact(calcHash))
			return false;

		// Protector verified. Mark it as consumed
		req.ConsumedPostFormSecurityElements().Add(piCandidate.m_element);
		return true;
	}


	void PostFormSecurity::ForEachProtectorInCookie(Seq cookie, std::function<bool(ProtectorInfo const&)> onProtector)
	{
		char const* const c_zSeparators = ": \t\r\n";

		while (true)
		{
			cookie.DropToFirstByteNotOf(c_zSeparators);
			if (!cookie.n)
				break;

			Seq protector = cookie.ReadToFirstByteOf(c_zSeparators);
			if (!protector.n)
				break;

			if (protector.n == ProtectorLen)
			{
				Seq reader = protector;
				Seq timeHex = reader.ReadBytes(16);
				uint64 timeUnixUnsigned = timeHex.ReadNrUInt64(16);
				if (!timeHex.n)
				{
					int64 timeUnixSigned = (int64) timeUnixUnsigned;
					if (timeUnixSigned > 0)
					{
						ProtectorInfo pi;
						pi.m_protector = protector;
						pi.m_t = Time::FromUnixTime(timeUnixSigned);
						pi.m_element = reader;
						pi.m_cookieToken = reader.ReadBytes(Token::Len);
						pi.m_bothTokensHash = reader;

						if (!onProtector(pi))
							break;
					}
				}
			}
		}
	}

}
