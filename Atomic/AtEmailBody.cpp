#include "AtIncludes.h"
#include "AtEmailBody.h"

#include "AtMarkdownTransform.h"


namespace At
{
	Str EncodeEmailTextBody_New(Markdown::Transform const& mkdn)
	{
		TextBuilder text { 2 * mkdn.Tree().Root().SrcText().n };
		mkdn.ToText(text);
		return text.Final();
	}


	Str EncodeEmailHtmlBody_New(Markdown::Transform const& mkdn)
	{
		HtmlBuilder html { 2 * mkdn.Tree().Root().SrcText().n };
		mkdn.ToHtml(html);
		return html.EndHtml().Final();
	}

/*
	Str EncodeEmailTextBody_ReplyOrForward(Markdown::Transform const& mkdn, Imf::Message const& origMsg, Seq origText)
	{
		
	}


	Str EncodeEmailHtmlBody_ReplyOrForward(Markdown::Transform const& mkdn, Imf::Message const& origMsg, ParseNode const& origHtmlNode)
	{
	}
*/
}
