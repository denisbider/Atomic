#pragma once

#include "AtImfReadWrite.h"
#include "AtMarkdownTransform.h"

namespace At
{
	Str EncodeEmailTextBody_New(Markdown::Transform const& mkdn);
	Str EncodeEmailHtmlBody_New(Markdown::Transform const& mkdn);

	Str EncodeEmailTextBody_ReplyOrForward(Markdown::Transform const& mkdn, Imf::Message const& origMsg, Seq origText);
	Str EncodeEmailHtmlBody_ReplyOrForward(Markdown::Transform const& mkdn, Imf::Message const& origMsg, ParseNode const& origHtmlNode);
}
