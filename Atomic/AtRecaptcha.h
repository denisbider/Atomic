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

		void Init(Seq siteKey, Seq secret);
		bool ProcessPost(HttpRequest const& req);	
		HtmlBuilder& Render(HtmlBuilder& html) const;

	private:
		Str  m_siteKey;
		Str  m_secret;
		Str  m_verifyError;
	};

}
