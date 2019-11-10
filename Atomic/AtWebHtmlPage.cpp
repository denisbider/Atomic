#include "AtIncludes.h"
#include "AtWebHtmlPage.h"


namespace At
{

	void WebHtmlPage::WP_GenResponse(HttpRequest& req)
	{
		if (m_response.StatusCode == 0)
			m_response.StatusCode = HttpStatus::OK;

		SetResponseContentType("text/html; charset=UTF-8");
		HtmlBuilder html;
		
		html.DefaultHead()
				.Title();
					WHP_Title(html)
				.EndTitle()
				.Method(*this, &WebHtmlPage::WHP_Head)
			.EndHead()
			.Body()
				.Method(*this, &WebHtmlPage::WHP_PreBody, req);

		if (m_errs.Any())
		{
			html.Div().Id("WHP_Errs");
			for (Str const& err : m_errs)
			{
				Seq trimmed = Seq(err).Trim();
				if (!trimmed.ContainsByte('\n'))
					html.P().Class("submitErr").T(trimmed).EndP();
				else
					html.Pre().Class("submitErr").T(err).EndPre();		// Preserve formatting for multiline error messages. Can contain significant indentation
			}
			html.EndDiv();
		}

		html	.Div().Id("WHP_Body")
					.Method(*this, &WebHtmlPage::WHP_Body, req)
				.EndDiv()
			.EndBody().EndHtml();
		
		if (html.MustSetPostFormSecurityCookie(req))
			html.SetPostFormSecurityCookie(req, *this);
		
		Seq csp = html.Csp().Final();
		if (csp.n)
			SetResponseHeader_ContentSecurityPolicy(csp);

		AddResponseBodyChunk(html.Final());
	}

}
