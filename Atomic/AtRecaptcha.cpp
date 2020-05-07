#include "AtIncludes.h"
#include "AtRecaptcha.h"

#include "AtJsonReadWrite.h"
#include "AtSocket.h"
#include "AtWinInet.h"
#include "AtUrlEncode.h"


namespace At
{
	
	FormInputType const fit_recaptchaKey ( "50", "100", 100 );


	void Recaptcha::ReInit(HttpRequest& req, Seq siteKey, Seq secret)
	{
		m_secure = req.IsSecure();
		m_siteKey = siteKey;
		m_secret = secret;
		m_verifyError.Clear();
	}


	bool Recaptcha::ProcessPost(HttpRequest& req)
	{
		Seq response = req.PostNvp("g-recaptcha-response").Trim();

		if (response.n && response.n <= MaxResponseParameterBytes)
		{
			Str post;
			post.ReserveExact(100 + m_secret.Len() + response.n)
				.Add("secret=").UrlEncode(m_secret)
				.Add("&remoteip=").UrlEncode(Str::From(req.RemoteAddr(), SockAddr::AddrOnly))
				.Add("&response=").UrlEncode(response);
		
			Str verifyResponse;
			{
				WinInet wi;
				wi.Init(INTERNET_OPEN_TYPE_DIRECT);
				wi.Connect("www.google.com", 443, INTERNET_FLAG_SECURE);
				wi.OpenRequest("POST", "/recaptcha/api/siteverify", Seq(), INTERNET_FLAG_SECURE);
				wi.SendRequest("Content-Type: application/x-www-form-urlencoded", post);
				wi.ReadResponse(verifyResponse, 500);
			}
			
			ParseTree pt { verifyResponse };
			if (!pt.Parse(Json::C_Json))
				m_verifyError = "Error parsing JSON response from reCAPTCHA server";
			else
			{
				ParseNode const* objectNode = pt.Root().DeepFind(Json::id_Object);
				if (!objectNode)
					m_verifyError = "No JSON object in response from reCAPTCHA server";
				else
				{
					bool haveSuccess {}, success {};

					Json::DecodeObject(*objectNode, [&] (ParseNode const&, Seq name, ParseNode const& valueNode) -> bool
						{
							if (name == "success")
							{
								if (!haveSuccess)
								{
									haveSuccess = true;
									success = Json::DecodeBool(valueNode);
								}
								else
								{
									success = false;
									m_verifyError.Add("JSON response from reCAPTCHA server contains multiple success values\r\n");
								}
							}
							else if (name == "error-codes")
							{
								m_verifyError.Add("Errors reported by reCAPTCHA server: ").Add(valueNode.SrcText()).Add("\r\n");
							}

							return true;
						} );

					if (!m_verifyError.Any())
						if (!haveSuccess)
							m_verifyError.Set("Missing success field in response from reCAPTCHA server");
						else if (!success)
							m_verifyError.Set("Unspecified error response from reCAPTCHA server");

					if (success)
						return true;
				}
			}
		}
	
		return false;
	}


	HtmlBuilder& Recaptcha::Render(HtmlBuilder& html) const
	{
		html.Div().Class("recaptcha")
				.ScriptSrc("https://www.google.com/recaptcha/api.js")
				.Div().Class("g-recaptcha").Data_SiteKey(m_siteKey).EndDiv()
			.EndDiv();

		html.Csp()
			.AddScriptSrc("https://www.google.com/recaptcha/")
			.AddScriptSrc("https://www.gstatic.com/recaptcha/")
			.AddFrameSrc("https://www.google.com/recaptcha/");

		return html;
	}

}
