#pragma once

#include "AtToken.h"
#include "AtWebRequestHandler.h"


namespace At
{

	class PostFormSecurity : public NoCopy
	{
	public:
		// Initializes the POST form cookie value and security field if not yet initialized.
		Seq PostFormSecurityField();

		// Returns true if the big POST form security cookie must be updated to account for either consumed
		// protectors or a new protector to be added as a result of processing the current request.
		bool MustSetPostFormSecurityCookie(HttpRequest const& req) const { return m_havePostFormSecurity || req.ConsumedPostFormSecurityElements().Any(); }

		// Sets the big POST form cookie to account for any protectors consumed. Adds new protector if any.
		// Must be called if MustSetPostFormSecurityCookie() returns true.
		void SetPostFormSecurityCookie(HttpRequest& req, WebRequestHandler& handler);

		// Gets the POST form security cookie if the provided request contains one. Returns empty Seq otherwise.
		static Seq GetPostFormSecurityCookie(HttpRequest const& req) { return req.CookiesNvp("PostForm"); }

		// Checks if the provided HttpRequest contains a matching POST form security cookie element and field.
		// If there is a match, the cookie element is marked as consumed for later removal in SetPostFormSecurityCookie().
		// If there is no match, the method returns false.
		static bool ConsumePostFormSecurityField(HttpRequest const& req);

	protected:
		enum { ProtectorExpiryDaysPast = 5, ProtectorExpiryDaysFuture = 1 };
		enum { SecurityElementLen = Token::Len + Token::TokenizedLen(32),
		       ProtectorLen = 16 + SecurityElementLen };

		bool	m_havePostFormSecurity {};
		Str		m_postFormCookieElement;					// Goes into HTTP header
		Str		m_postFormSecurityField;					// Goes into HTML content

		struct ProtectorInfo
		{
			Seq  m_protector;
			Time m_t;
			Seq  m_element;
			Seq  m_cookieToken;
			Seq  m_bothTokensHash;

			bool Any() const { return m_protector.Any(); }

			bool IsExpired(Time now) const
				{ return (m_t + Time::FromDays(ProtectorExpiryDaysPast) < now) ||
						 (m_t > now + Time::FromDays(ProtectorExpiryDaysFuture)); }

			Time Key() const { return m_t; }
		};

		// Enumerates protectors in a cookie. The onProtector lambda accepts parameters protector, time, cookieElement.
		// The onProtector lambda should return true to continue processing or false to break.
		// Any blatantly malformed protectors are skipped and the onProtector lambda is not called for them.
		static void ForEachProtectorInCookie(Seq cookie, std::function<bool(ProtectorInfo const&)> onProtector);
	};

}
