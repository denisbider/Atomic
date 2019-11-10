#pragma once

#include "AtParse.h"
#include "AtHtmlGrammar.h"
#include "AtUriGrammar.h"

namespace At
{
	namespace Markdown
	{
		// Connecting items
		DECL_RUID(LineEnd)
		DECL_RUID(Junk)

		// Paragraph-level items
		DECL_RUID(Text)
		DECL_RUID(Code)
		DECL_RUID(EscPair)
		DECL_RUID(Entity)
		DECL_RUID(Bold)
		DECL_RUID(Italic)

		// Block-level items
		DECL_RUID(BlankLine)
		DECL_RUID(QuoteBlock)
		DECL_RUID(ListItemNr)
		DECL_RUID(ListKind)
		DECL_RUID(ListKindBullet)
		DECL_RUID(ListKindBulletDash)
		DECL_RUID(ListKindBulletPlus)
		DECL_RUID(ListKindBulletStar)
		DECL_RUID(ListKindOrdered)
		DECL_RUID(ListKindOrderedDot)
		DECL_RUID(ListKindOrderedCBr)
		DECL_RUID(ListItem)
		DECL_RUID(List)
		DECL_RUID(ListLoose)
		DECL_RUID(CodeBlock)
		DECL_RUID(HorizRule)
		DECL_RUID(Link)
		DECL_RUID(Heading)
		DECL_RUID(Heading1)
		DECL_RUID(Heading2)
		DECL_RUID(Heading3)
		DECL_RUID(Heading4)
		DECL_RUID(Paragraph)

		bool C_Document(ParseNode& p);
	}
}
