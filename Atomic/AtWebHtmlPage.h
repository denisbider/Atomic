#include "AtHtmlBuilder.h"
#include "AtWebPage.h"


namespace At
{

	class WebHtmlPage : virtual public WebPage
	{
	public:
		ReqResult WP_Process     (EntityStore& store, HttpRequest& req) override final;
		void      WP_GenResponse (HttpRequest& req) override final;
	
		virtual ReqResult WHP_Process(EntityStore& store, HttpRequest& req) = 0;

		// Used to build HTML response when WHP_Process returned true.
		virtual HtmlBuilder& WHP_Title   (HtmlBuilder& html) const = 0;
		virtual HtmlBuilder& WHP_Head    (HtmlBuilder& html) const { return html; }
		virtual HtmlBuilder& WHP_PreBody (HtmlBuilder& html, HttpRequest&) const { return html; }
		virtual HtmlBuilder& WHP_Body    (HtmlBuilder& html, HttpRequest& req) const = 0;

	private:
		Vec<Str> m_errs;
		Str      m_cfmMsg;

	protected:
		Vec<Str>&  AccessPageErrs ()             { return m_errs; }
		bool       AnyPageErrs    () const       { return m_errs.Any(); }
		ReqResult  AddPageErr     (Str const& s) { m_errs.Add(s);                    return ReqResult::Continue; }
		ReqResult  AddPageErr     (Str&& s)      { m_errs.Add(std::forward<Str>(s)); return ReqResult::Continue; }
		Seq        CfmMsg         () const       { return m_cfmMsg; }
	};

}
