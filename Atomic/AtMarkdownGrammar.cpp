#include "AtIncludes.h"
#include "AtMarkdownGrammar.h"

#include "AtUnicode.h"
#include "AtUtf8.h"

namespace At
{
	using namespace Parse;

	namespace Markdown
	{
		// This grammar loosely follows CommonMark in SOME respects - current latest version 0.22:
		//
		// http://spec.commonmark.org/0.22/
		//
		// However, it is not the intent of this markdown to be a general purpose authoring format. HTML or similar is better suited for that.
		// Instead, this markdown is intended for use in emails and online forums. Therefore, it has major differences compared to CommonMark.
		//
		// Structural differences:
		//
		// - HTML blocks are not supported.
		// - Headings are not supported. (Users generally don't know about them, and don't expect them when they accidentally take effect)
		// - Inline links (within paragraph) are not supported. Instead, plain links are supported at block scope, when alone on a line.
		// - Link reference definitions are not supported.
		// - Fenced code blocks are not supported. (But indented code blocks are, as well as code spans.)
		// 
		// Subtler block level differences:
		//
		// - Lazy continuation lines are not supported. (Frequent cause of incorrect quoting where markdown is in popular use)
		// - Single newline + indent of 2+ columns separates paragraphs. (Joining that is frequent cause of incorrect formatting where markdown is in popular use)
		//   Paragraph lines separated by a single newline; but no more than one space in front of next line; are still joined.
		//
		// Paragraph level differences:
		//
		// - Emphasis markers are * for em/italic, and ** or __ for strong/bold. A single underscore (_) is NOT interpreted as emphasis start/end.
		// - Only one level of emphasis nesting is supported - bold within italic, or italic within bold. Nesting must be non-overlapping.
		// 
		// This grammar assumes input is UTF-8; it should be converted to UTF-8 if it's received in a different encoding. Invalid
		// UTF-8 is NOT accepted because it would complicate the grammar considerably, and make use of the parse tree dangerous.
		// If invalid UTF-8 should be tolerated, excise it from the input before passing the input to the grammar.

		// Connecting items
		DEF_RUID_B(LineEnd)
		DEF_RUID_B(Junk)
		DEF_RUID_X(EmphStart, id_Junk)
		DEF_RUID_X(EmphEnd,   id_Junk)

		// Paragraph-level items
		DEF_RUID_B(Text)
		DEF_RUID_B(Code)
		DEF_RUID_B(EscPair)
		DEF_RUID_B(Bold)
		DEF_RUID_B(Italic)

		// Block-level items
		DEF_RUID_B(BlankLine)
		DEF_RUID_B(QuoteBlock)
		DEF_RUID_B(ListItemNr)
		DEF_RUID_B(ListKind)
		DEF_RUID_X(ListKindBullet,     id_ListKind)
		DEF_RUID_X(ListKindBulletDash, id_ListKindBullet)
		DEF_RUID_X(ListKindBulletPlus, id_ListKindBullet)
		DEF_RUID_X(ListKindBulletStar, id_ListKindBullet)
		DEF_RUID_X(ListKindOrdered,    id_ListKind)
		DEF_RUID_X(ListKindOrderedDot, id_ListKindOrdered)
		DEF_RUID_X(ListKindOrderedCBr, id_ListKindOrdered)
		DEF_RUID_B(ListItem)
		DEF_RUID_B(List)
		DEF_RUID_X(ListLoose, id_List)
		DEF_RUID_B(CodeBlock)
		DEF_RUID_B(HorizRule)
		DEF_RUID_B(Link)
		DEF_RUID_B(Heading)
		DEF_RUID_X(Heading1, id_Heading)
		DEF_RUID_X(Heading2, id_Heading)
		DEF_RUID_X(Heading3, id_Heading)
		DEF_RUID_X(Heading4, id_Heading)
		DEF_RUID_B(Paragraph)

		bool V_OptCr_Lf    (ParseNode& p)                   { return G_Req<0,1>    (p, id_Append,   V_Cr, V_Lf                    ); }
		bool V_Cr_NfbLf    (ParseNode& p)                   { return V_ByteNfb     (p,              13, 10                        ); }
		bool V_LineEnd     (ParseNode& p)                   { return G_Choice      (p, id_Append,   N_End, V_OptCr_Lf, V_Cr_NfbLf ); }
		bool C_LineEnd     (ParseNode& p)                   { return G_Req         (p, id_LineEnd,  V_LineEnd                     ); }

		bool G_HWs_LineEnd (ParseNode& p, Ruid const& type) { return G_Req<0,1>    (p, type,        V_HWs, V_LineEnd              ); }
		bool V_HWs_LineEnd (ParseNode& p)                   { return G_HWs_LineEnd (p, id_Append                                  ); }
		bool C_HWs_LineEnd (ParseNode& p)                   { return G_HWs_LineEnd (p, id_LineEnd                                 ); }

		bool V_NonWsChar(ParseNode& p) { return V_Utf8CharIf(p, [] (uint c) -> bool { return c==32 || (c>=9 && c<=13); } ); }

		bool G_QuoteBlockMarker(ParseNode& p, Ruid const& type) { return G_Req<0,1,0>(p, type, V_HWs_Indent<1,3>, V_Grtr, V_Space); }
		bool V_QuoteBlockMarker(ParseNode& p) { return G_QuoteBlockMarker(p, id_Append); }



		// Paragraph text

		uint EmphCharCat(uint c)
		{
			if (!Unicode::IsValidCodePoint(c))
				return 0;

			if (c == 10 || c == 13)
				return 1;

			if (c == 9)
				return 2;

			uint cat { Unicode::CharInfo::Get(c).m_cat };
			switch (cat)
			{
			case Unicode::Category::Separator_Space:
				return 2;

			case Unicode::Category::Punctuation_Connector:		
			case Unicode::Category::Punctuation_Dash:			
			case Unicode::Category::Punctuation_Close:			
			case Unicode::Category::Punctuation_FinalQuote:		
			case Unicode::Category::Punctuation_InitialQuote:	
			case Unicode::Category::Punctuation_Other:			
			case Unicode::Category::Punctuation_Open:			
				return 3;

			default:
				return 4;
			}
		}


		struct MarkerType { enum E { Start, End }; };

		bool C_EmphMarker(ParseNode& p, Seq marker, MarkerType::E markerType)
		{
			Seq reader { p.Remaining() };
			if (!reader.StripPrefixExact(marker))
				return false;

			uint nextChar;
			Utf8::ReadCodePoint(reader, nextChar);
			uint nextCharCat { EmphCharCat(nextChar) };

			Seq before { p.BeforeRemaining() };
			uint prevChar;
			Utf8::RevReadCodePoint(before, prevChar);
			uint prevCharCat { EmphCharCat(prevChar) };

			Ruid const* type;
			if (markerType == MarkerType::Start)
			{
				type = &id_EmphStart;
				if (prevCharCat > nextCharCat)
					return false;					// Marker is not left-flanking
			}
			else
			{
				type = &id_EmphEnd;
				if (prevCharCat < nextCharCat)
					return false;					// Marker is not right-flanking
			}
			
			ParseNode* pn = p.NewChild(*type);
			if (!pn)
				return false;

			pn->ConsumeUtf8Seq(marker.n);
			return p.CommitChild(pn);
		}


		bool C_CodeSpan(ParseNode& p)
		{
			if (!p.Remaining().StartsWithExact("`"))
				return false;

			ParseNode* pnSpan = p.NewChild(id_Append);
			if (!pnSpan)
				return false;

			if (!G_Repeat(*pnSpan, id_Junk, V_Backtick))
				return p.FailChild(pnSpan);

			Seq delimiter { pnSpan->LastChild().SrcText() };
			Seq reader { pnSpan->Remaining() };
			while (true)
			{
				if (!reader.n ||
					reader.p[0] == 13 ||
					reader.p[0] == 10 ||
					reader.StartsWithExact(delimiter))
					break;

				uint c { reader.ReadUtf8Char() };
				if (c == UINT_MAX)
					return p.FailChild(pnSpan);
			}

			sizet nrCodeBytes { NumCast<sizet>(reader.p - pnSpan->Remaining().p) };
			if (!nrCodeBytes)
				return p.FailChild(pnSpan);

			ParseNode* pnCode = pnSpan->NewChild(id_Code);
			if (!pnCode)
				return p.FailChild(pnSpan);

			pnCode->ConsumeUtf8Seq(nrCodeBytes);
			pnSpan->CommitChild(pnCode);

			if (!G_SeqMatchExact(*pnSpan, id_Junk, delimiter))
				return p.FailChild(pnSpan);

			return p.CommitChild(pnSpan);
		}


		bool V_Escapable          (ParseNode& p) { return V_Utf8CharIf  (p,                [] (uint c) -> bool { return !!ZChr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", c); }                                  ); }
		bool C_EscPair            (ParseNode& p) { return G_Req<1,1>    (p, id_EscPair,    V_Backslash, V_Escapable                                                                                         ); }
																																															              	 
		bool V_TextNonSpecialChar (ParseNode& p) { return V_Utf8CharIf  (p,                [] (uint c) -> bool { return !ZChr("\\&`*_\r\n", c); }                                                           ); }
		bool C_TextNonSpecial     (ParseNode& p) { return G_Repeat      (p, id_Text,       V_TextNonSpecialChar                                                                                             ); }
																																															              
		bool V_TextNonWsChar      (ParseNode& p) { return V_Utf8CharIf  (p,                [] (uint c) -> bool { return c!=13 &&c!=10; }                                                                    ); }
		bool C_TextNonWsChar      (ParseNode& p) { return G_Req         (p, id_Text,       V_TextNonWsChar                                                                                                  ); }
																																															              
		bool C_ParaEl_NoEmph      (ParseNode& p) { return G_Choice      (p, id_Append,     C_TextNonSpecial, C_HWs, C_EscPair, Html::C_CharRef, C_CodeSpan                                                  ); }
																																															                
		bool C_BUndStart          (ParseNode& p) { return C_EmphMarker  (p,                "__", MarkerType::Start                                                                                          ); }
		bool C_BUndEnd            (ParseNode& p) { return C_EmphMarker  (p,                "__", MarkerType::End                                                                                            ); }
		bool C_BAstStart          (ParseNode& p) { return C_EmphMarker  (p,                "**", MarkerType::Start                                                                                          ); }
		bool C_BAstEnd            (ParseNode& p) { return C_EmphMarker  (p,                "**", MarkerType::End                                                                                            ); }
		bool C_ItalicStart        (ParseNode& p) { return C_EmphMarker  (p,                "*",  MarkerType::Start                                                                                          ); }
		bool C_ItalicEnd          (ParseNode& p) { return C_EmphMarker  (p,                "*",  MarkerType::End                                                                                            ); }
																																															     
		bool C_Italic_NoBold      (ParseNode& p) { return G_Req<1,1>    (p, id_Italic,     C_ItalicStart, C_UntilIncl<G_Choice<C_ParaEl_NoEmph,                  C_ItalicEnd, C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_BUnd_NoItalic      (ParseNode& p) { return G_Req<1,1>    (p, id_Bold,       C_BUndStart,   C_UntilIncl<G_Choice<C_ParaEl_NoEmph,                  C_BUndEnd,   C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_BAst_NoItalic      (ParseNode& p) { return G_Req<1,1>    (p, id_Bold,       C_BAstStart,   C_UntilIncl<G_Choice<C_ParaEl_NoEmph,                  C_BAstEnd,   C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_Bold_NoItalic      (ParseNode& p) { return G_Choice      (p, id_Append,     C_BUnd_NoItalic, C_BAst_NoItalic                                                                                 ); }

		bool C_Italic_AllowBold   (ParseNode& p) { return G_Req<1,1>    (p, id_Italic,     C_ItalicStart, C_UntilIncl<G_Choice<C_ParaEl_NoEmph, C_Bold_NoItalic, C_ItalicEnd, C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_BUnd_AllowItalic   (ParseNode& p) { return G_Req<1,1>    (p, id_Bold,       C_BUndStart,   C_UntilIncl<G_Choice<C_ParaEl_NoEmph, C_Italic_NoBold, C_BUndEnd  , C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_BAst_AllowItalic   (ParseNode& p) { return G_Req<1,1>    (p, id_Bold,       C_BAstStart,   C_UntilIncl<G_Choice<C_ParaEl_NoEmph, C_Italic_NoBold, C_BAstEnd  , C_TextNonWsChar>, id_EmphEnd> ); }
		bool C_Bold_AllowItalic   (ParseNode& p) { return G_Choice      (p, id_Append,     C_BUnd_AllowItalic, C_BAst_AllowItalic                                                                           ); }
																																														     
		bool C_ParaEl             (ParseNode& p) { return G_Repeat      (p, id_Append,     G_Choice<C_ParaEl_NoEmph, C_Bold_AllowItalic, C_Italic_AllowBold, C_TextNonWsChar>                               ); }
		bool C_ParagraphLine      (ParseNode& p) { return G_Req<0,1,1>  (p, id_Append,     C_HWs, C_ParaEl, C_LineEnd                                                                                       ); }



		// Block parsing

		struct BlockContCrit : NoCopy
		{
			// Implements a stack-based linked list of block continuation criteria

			enum EQuote  { Quote = 1 };
			enum EIndent { Indent = 2 };

			BlockContCrit(EQuote, BlockContCrit* parent) : BlockContCrit(Quote, 0, parent) {}

			BlockContCrit(int type, sizet param, BlockContCrit* parent) : m_type(type), m_param(param), m_parent(parent)
			{
				if (parent)
				{
					EnsureAbort(!parent->m_next);
					m_first = parent->m_first;
					parent->m_next = this;
				}
				else
					m_first = this;
			}

			~BlockContCrit() { if (m_parent) m_parent->m_next = nullptr; }

			static bool Parse(ParseNode& p, BlockContCrit* contCrit)
			{
				if (!contCrit) return true;
				return contCrit->Parse(p);
			}

			bool Parse(ParseNode& p)
			{
				// Verify block continuation criteria starting from deepest parent
				if (m_parent)
					return m_first->Parse(p);
				
				ParseNode* pn = p.NewChild(id_Junk);
				if (!pn)
					return false;

				BlockContCrit* cur { this };
				do
				{
					if (!cur->ParseCrit(*pn))
						return p.FailChild(pn);
					cur = cur->m_next;
				}
				while (cur); 

				return p.CommitChild(pn);
			}

			bool ParseCrit(ParseNode& p)
			{
				if (m_type == Quote)
					return V_QuoteBlockMarker(p);

				if (m_type == Indent)
				{
					if (V_HWs_Indent(p, m_param, m_param))
						return true;

					// Succeed for a blank line, even if it doesn't have sufficient indentation
					V_HWs_Indent(p, 1, m_param);

					ParseNode* pnEnd = p.NewChild(id_Append);
					if (!pnEnd)
						return false;

					bool atEnd { V_LineEnd(*pnEnd) || N_End(*pnEnd) };
					p.DiscardChild(pnEnd);
					return atEnd;
				}

				EnsureThrow(!"Unrecognized block continuation criterion type");
				return false;
			}

			int m_type;
			sizet m_param;

			BlockContCrit* m_parent;	// nullptr when this is first criterion
			BlockContCrit* m_first;		// nullptr when this is first criterion
			BlockContCrit* m_next {};
		};


		enum class BlockParseScope { Whole, Start };
		typedef bool (*BlockParseFunc)(ParseNode&, BlockContCrit*, BlockParseScope);

		static bool C_CritAndBlock(ParseNode& p, BlockContCrit* contCrit, BlockParseFunc bpf, BlockParseScope scope)
		{
			ParseNode* pnAppend = p.NewChild(id_Append);
			if (!pnAppend)
				return false;

			if (!BlockContCrit::Parse(*pnAppend, contCrit) ||
				!bpf(*pnAppend, contCrit, scope))
				return p.FailChild(pnAppend);

			return p.CommitChild(pnAppend);
		}


		inline bool C_BlankLine(ParseNode& p)                                  { return G_HWs_LineEnd(p, id_BlankLine); }
		inline bool C_BlankLine(ParseNode& p, BlockContCrit*, BlockParseScope) { return G_HWs_LineEnd(p, id_BlankLine); }

		       bool C_Block(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope);
		inline bool C_Block(ParseNode& p) { return C_Block(p, nullptr, BlockParseScope::Whole); }

		bool C_Document(ParseNode& p) { return G_UntilExcl(p, id_Append, C_Block, N_End, 0); }



		// QuoteBlock

		bool C_QuoteBlock(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			ParseNode* pnQuote = p.NewChild(id_QuoteBlock);
			if (!pnQuote)
				return false;

			// First child block
			if (!G_QuoteBlockMarker(*pnQuote, id_Junk))
				return p.FailChild(pnQuote);

			BlockContCrit thisBlockContCrit { BlockContCrit::Quote, contCrit };			
			if (!C_Block(*pnQuote, &thisBlockContCrit, scope))
				return p.FailChild(pnQuote);

			// Subsequent child blocks
			if (BlockParseScope::Whole == scope)
			{
				while (true)
				{
					ParseNode* pnChildBlock = pnQuote->NewChild(id_Append);
					if (!pnChildBlock)
						return p.FailChild(pnQuote);

					if (!C_CritAndBlock(*pnChildBlock, &thisBlockContCrit, C_Block, scope))
						{ pnQuote->FailChild(pnChildBlock); break; }

					pnQuote->CommitChild(pnChildBlock);
				}
			}
				
			return p.CommitChild(pnQuote);
		}



		// ListItemBlock

		bool G_ListBulletMarker  (ParseNode& p, Ruid const& type, ParseFunc pf) { return G_Req<0,1,1>        (p, type,                  V_HWs_Indent<1,3>, pf, V_AsciiHorizWhitespace ); }
		bool C_ListBulletDash    (ParseNode& p)                                 { return G_ListBulletMarker  (p, id_ListKindBulletDash, V_Dash                                        ); }
		bool C_ListBulletPlus    (ParseNode& p)                                 { return G_ListBulletMarker  (p, id_ListKindBulletPlus, V_Plus                                        ); }
		bool C_ListBulletStar    (ParseNode& p)                                 { return G_ListBulletMarker  (p, id_ListKindBulletStar, V_Star                                        ); }
																											 
		bool C_ListItemNr        (ParseNode& p)                                 { return G_Repeat            (p, id_ListItemNr,         V_AsciiDecDigit, 1, 9                         ); }
		bool C_Dot_HWsByte       (ParseNode& p)                                 { return G_Req<1,1>          (p, id_Junk,               V_Dot, V_AsciiHorizWhitespace                 ); }
		bool C_CBr_HWsByte       (ParseNode& p)                                 { return G_Req<1,1>          (p, id_Junk,               V_CloseBr, V_AsciiHorizWhitespace             ); }
		bool G_ListOrderedMarker (ParseNode& p, Ruid const& type, ParseFunc pf) { return G_Req<0,1,1>        (p, type,                  C_HWs_Indent<1,3>, C_ListItemNr, pf           ); }
		bool C_ListOrderedDot    (ParseNode& p)                                 { return G_ListOrderedMarker (p, id_ListKindOrderedDot, C_Dot_HWsByte                                 ); }
		bool C_ListOrderedCBr    (ParseNode& p)                                 { return G_ListOrderedMarker (p, id_ListKindOrderedCBr, C_CBr_HWsByte                                 ); }


		bool C_ListItemBlock(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			bool loose {};
			ParseNode* pnListItem = p.NewChild(id_ListItem);
			if (!pnListItem)
				return false;

			// First child block
			if (!C_ListBulletDash(*pnListItem) &&
				!C_ListBulletPlus(*pnListItem) &&
				!C_ListBulletStar(*pnListItem) &&
				!C_ListOrderedDot(*pnListItem) &&
				!C_ListOrderedCBr(*pnListItem))
				return p.FailChild(pnListItem);

			// Base list item indent equals length of list item marker including leading whitespace, and up to one trailing whitespace
			sizet indent { pnListItem->LastChild().SrcText().n };

			// Indent may include an additional 1-3 spaces if remainder of first line is not blank, or start of a code block
			Seq remaining { pnListItem->Remaining() };
			Seq additionalHWs { remaining.ReadToFirstByteNotOf(" \t") };
			if (additionalHWs.n >= 1 && additionalHWs.n <= 3 && remaining.n && remaining.p[0] != 13 && remaining.p[0] != 10)
				indent += additionalHWs.n;

			BlockContCrit thisBlockContCrit { BlockContCrit::Indent, indent, contCrit };
			if (!C_Block(*pnListItem, &thisBlockContCrit, scope))
				return p.FailChild(pnListItem);

			if (!pnListItem->LastChild().IsType(id_Paragraph))
				loose = true;

			// Subsequent child blocks
			if (BlockParseScope::Whole == scope)
			{
				while (true)
				{
					ParseNode* pnChildBlock = pnListItem->NewChild(id_Append);
					if (!pnChildBlock)
						return p.FailChild(pnListItem);

					if (!C_CritAndBlock(*pnChildBlock, &thisBlockContCrit, C_Block, scope))
						{ pnListItem->FailChild(pnChildBlock); break; }

					if (pnChildBlock->LastChild().IsType(id_BlankLine))
					{
						if (!C_CritAndBlock(*pnChildBlock, &thisBlockContCrit, C_Block, scope))
							{ pnListItem->FailChild(pnChildBlock); break; }

						if (pnChildBlock->LastChild().IsType(id_BlankLine))
							{ pnListItem->FailChild(pnChildBlock); break; }
					}

					loose = true;
					pnListItem->CommitChild(pnChildBlock);
				}
			}

			if (loose)				
				pnListItem->RefineParentType(id_List, id_ListLoose);
			return p.CommitChild(pnListItem);
		}



		// ListBlock

		bool C_ListBlock(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			ParseNode* pnList = p.NewChild(id_List);
			if (!pnList)
				return false;

			// First list item
			if (!C_ListItemBlock(*pnList, contCrit, scope))
				return p.FailChild(pnList);

			if (BlockParseScope::Whole == scope)
			{
				// First child is id_ListItem; first child of that is the kind of list item
				Ruid const& firstListItemKind { pnList->FirstChild().FirstChild().Type() };

				// Subsequent list items
				while (true)
				{
					ParseNode* pnAppend = pnList->NewChild(id_Append);
					if (!pnAppend)
						return p.FailChild(pnList);
				
					// Accept up to one blank line between list items
					if (C_CritAndBlock(*pnAppend, contCrit, C_BlankLine, scope))
						pnList->RefineType(id_ListLoose);

					if (!C_CritAndBlock(*pnAppend, contCrit, C_ListItemBlock, scope))
						{ pnList->FailChild(pnAppend); break; }

					Ruid const& listItemKind { pnAppend->FlatFindRef(id_ListItem).FirstChild().Type() };
					if (!listItemKind.Equal(firstListItemKind))
						{ pnList->FailChild(pnAppend); break; }

					pnList->CommitChild(pnAppend);
				}
			}

			return p.CommitChild(pnList);
		}



		// CodeBlock

		bool C_CodeBlock(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			ParseNode* pnBlock = p.NewChild(id_CodeBlock);
			if (!pnBlock)
				return false;

			// First line
			if (!C_HWs_Indent(*pnBlock, id_HWs, 4, 4))
				return p.FailChild(pnBlock);

			if (!G_UntilExcl(*pnBlock, id_Code, V_Utf8CharAny, V_HWs_LineEnd))
				return p.FailChild(pnBlock);

			if (!C_HWs_LineEnd(*pnBlock))
				return p.FailChild(pnBlock);

			if (BlockParseScope::Whole == scope)
			{
				// Subsequent lines
				while (true)
				{
					ParseNode* pnAppend = pnBlock->NewChild(id_Append);
					if (!pnAppend)
						return p.FailChild(pnBlock);

					// Read until next fully indented line, if any
					enum State { Invalid, Mismatch, Continue, ReachedEnd } state = Invalid;
					while (true)
					{
						if (!BlockContCrit::Parse(*pnAppend, contCrit))
							{ state = Mismatch; break; }

						if (C_HWs_Indent(*pnAppend, id_HWs, 4, 4))
						{
							// Fully indented line
							G_UntilExcl(*pnAppend, id_Code, V_Utf8CharAny, V_HWs_LineEnd);

							if (!C_HWs_LineEnd(*pnAppend))
								{ state = Mismatch; break; }

							state = Continue;
							break;
						}

						// Non-fully indented line
						C_HWs_Indent(*pnAppend, id_HWs, 1, 4);

						if (N_End(*pnAppend))
							{ state = ReachedEnd; break; }

						if (!G_Req(*pnAppend, id_LineEnd, V_LineEnd))
							{ state = Mismatch; break; }
					}

					EnsureThrow(state != Invalid);

					if (state == Mismatch)
						pnBlock->FailChild(pnAppend);
					else
						pnBlock->CommitChild(pnAppend);

					if (state != Continue)
						break;
				}
			}

			return p.CommitChild(pnBlock);
		}



		// HorizRule

		template <byte B> inline bool V_HorizRuleParticle(ParseNode& p) { return G_Req<1,0>(p, id_Append, V_ByteIs<B>, V_HWs); }
		
		bool C_HorizRule(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_HorizRule);
			if (!pn)
				return false;

			C_HWs_Indent(*pn, id_HWs, 1, 3);

			if (!G_Repeat(*pn, id_Junk, V_HorizRuleParticle<'-'>, 3) &&
				!G_Repeat(*pn, id_Junk, V_HorizRuleParticle<'_'>, 3) &&
				!G_Repeat(*pn, id_Junk, V_HorizRuleParticle<'*'>, 3))
				return p.FailChild(pn);

			if (!C_LineEnd(*pn))	// Any preceding HWs consumed by V_HorizRuleParticle
				return p.FailChild(pn);

			return p.CommitChild(pn);
		}



		// Link
		
		bool C_Link(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_Link);
			if (!pn)
				return false;

			C_HWs_Indent(*pn, id_HWs, 1, 3);

			bool haveUrl {};
			if (pn->Remaining().StartsWithInsensitive("http:") ||
				pn->Remaining().StartsWithInsensitive("https:"))
			{
				haveUrl = Uri::C_URI_reference(*pn);
			}

			if (!haveUrl)
				return p.FailChild(pn);

			if (!C_HWs_LineEnd(*pn))
				return p.FailChild(pn);

			return p.CommitChild(pn);
		}



		// Heading

		bool C_HeadingPrefix(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_Junk);
			if (!pn)
				return false;

			V_HWs_Indent(*pn, 1, 3);

			enum { NrHdgTypes = 4 };
			Ruid const* hdgTypes[NrHdgTypes] = { &id_Heading1, &id_Heading2, &id_Heading3, &id_Heading4 };

			sizet i = 0;
			while (true)
			{
				if (!Parse::V_Hash(*pn)) return p.FailChild(pn);
				if (V_HWs(*pn))          break;
				if (++i == NrHdgTypes)   return p.FailChild(pn);
			}

			EnsureThrow(i < NrHdgTypes);
			p.RefineType(*hdgTypes[i]);

			return p.CommitChild(pn);
		}

		bool C_Heading(ParseNode& p) { return G_Req<1,1,1>(p, id_Heading, C_HeadingPrefix, C_ParaEl, C_HWs_LineEnd); }



		// Paragraph

		bool C_Paragraph(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			ParseNode* pnPara = p.NewChild(id_Paragraph);
			if (!pnPara)
				return false;

			// First line
			if (!C_ParagraphLine(*pnPara))
				return p.FailChild(pnPara);

			if (BlockParseScope::Whole == scope)
			{
				// Subsequent lines
				while (true)
				{
					ParseNode* pnAppend = pnPara->NewChild(id_Append);
					if (!pnAppend)
						return p.FailChild(pnPara);

					if (!BlockContCrit::Parse(*pnAppend, contCrit))
						{ pnPara->FailChild(pnAppend); break; }

					if (C_HWs_Indent(*pnAppend, id_HWs, 2, SIZE_MAX))
						{ pnPara->FailChild(pnAppend); break; }

					ParseNode* pnDiscard = pnAppend->NewChild(id_Discard);
					if (!pnDiscard)
						{ pnPara->FailChild(pnAppend); return p.FailChild(pnPara); }

					bool isContinuation { C_Block(*pnDiscard, contCrit, BlockParseScope::Start) && pnDiscard->FirstChild().IsType(id_Paragraph) };
					pnAppend->DiscardChild(pnDiscard);
					if (!isContinuation)
						{ pnPara->FailChild(pnAppend); break; }

					EnsureThrow(C_ParagraphLine(*pnAppend));
					pnPara->CommitChild(pnAppend);
				}
			}

			return p.CommitChild(pnPara);
		}
		


		// Block

		bool C_Block(ParseNode& p, BlockContCrit* contCrit, BlockParseScope scope)
		{
			if (             N_End          (p                  )) return false;
			if (             C_BlankLine    (p                  )) return true;
			if (             C_QuoteBlock   (p, contCrit, scope )) return true;
			if (             C_ListBlock    (p, contCrit, scope )) return true;
			if (             C_CodeBlock    (p, contCrit, scope )) return true;
			if (             C_HorizRule    (p                  )) return true;
			if (             C_Link         (p                  )) return true;
			if (!contCrit && C_Heading      (p                  )) return true;
			if (             C_Paragraph    (p, contCrit, scope )) return true;
			return false;
		}

	}
}
