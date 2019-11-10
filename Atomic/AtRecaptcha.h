#pragma once

#include "AtHtmlBuilder.h"
#include "AtHttpRequest.h"


namespace At
{

	extern FormInputType const fit_recaptchaKey;


	class Recaptcha
	{
	public:
		enum { MaxResponseParameterBytes = 1000 };

		Recaptcha() : m_secure(false) {}

		void ReInit(HttpRequest& req, Seq siteKey, Seq secret);
		bool ProcessPost(HttpRequest& req);	
		HtmlBuilder& Render(HtmlBuilder& html) const;

	private:
		bool m_secure;
		Str m_siteKey;
		Str m_secret;
		Str m_verifyError;
	};

}
