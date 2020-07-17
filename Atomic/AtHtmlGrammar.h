#pragma once

#include "AtParse.h"
#include "AtHtmlCharRefs.h"

namespace At
{
	namespace Html
	{
		DECL_RUID(HtmlWs)
		DECL_RUID(Text)
		DECL_RUID(CharRef)
		DECL_RUID(CharRefName)
		DECL_RUID(CharRefDec)
		DECL_RUID(CharRefHex)
		DECL_RUID(Comment)

		DECL_RUID(CDataStart)
		DECL_RUID(CDataContent)
		DECL_RUID(CDataEnd)
		DECL_RUID(CData)

		DECL_RUID(QStr)

		DECL_RUID(AttrName)
		DECL_RUID(AttrValUnq)
		DECL_RUID(Attr)

		DECL_RUID(Tag)
		DECL_RUID(StartTag)
		DECL_RUID(LessSlash)
		DECL_RUID(Ws_Grtr)
		DECL_RUID(EndTag)

		DECL_RUID(RawText)
		DECL_RUID(SpecialElem)
		DECL_RUID(RawTextElem)
		DECL_RUID(Script)
		DECL_RUID(Style)
		DECL_RUID(RcdataElem)
		DECL_RUID(Title)
		DECL_RUID(TextArea)

		DECL_RUID(TrashTag)
		DECL_RUID(SuspectChar)

		extern char const* const c_htmlWsChars;

		bool C_HtmlWs   (ParseNode& p);
		bool C_CharRef  (ParseNode& p);
		bool C_Document (ParseNode& p);

	}
}
