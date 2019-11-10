#include "AtIncludes.h"
#include "AtMarkdownTransform.h"

#include "AtUnicode.h"
#include "AtUtf8.h"

namespace At
{
	namespace Markdown
	{

		HtmlBuilder& Transform::ToHtml(HtmlBuilder& html, ParseNode const& containerNode) const
		{
			for (ParseNode const& blockNode : containerNode)
			{
				if (blockNode.IsType(id_QuoteBlock))
				{
					html.Blockquote();
					ToHtml(html, blockNode);
					html.EndBlockquote();
				}
				else if (blockNode.IsType(id_List))
				{
					bool isLoose   { blockNode.IsType(id_ListLoose) };
					bool isOpened  {};
					bool isOrdered {};

					for (ParseNode const& childNode : blockNode)
					{
						if (childNode.IsType(id_ListItem))
						{
							if (!isOpened)
							{
								ParseNode const& pnListKind { childNode.FirstChild() };

								if (pnListKind.IsType(id_ListKindBullet))
								{
									html.Ul();
								}
								else if (pnListKind.IsType(id_ListKindOrdered))
								{
									isOrdered = true;
									html.Ol();

									Seq startNr { pnListKind.FlatFindRef(id_ListItemNr).SrcText() };
									if (!startNr.EqualExact("1"))
										html.Start(startNr);
								}

								isOpened = true;
							}

							html.Li();
							
							if (!isLoose)
								ParaElemsToHtml(html, childNode.FlatFindRef(id_Paragraph));
							else
								ToHtml(html, childNode);

							html.EndLi();
						}
						else
							EnsureThrow(childNode.IsType(id_BlankLine) || childNode.IsType(id_Junk));
					}

					if (isOpened)
						if (isOrdered)
							html.EndOl();
						else
							html.EndUl();
				}
				else if (blockNode.IsType(id_CodeBlock))
				{
					html.Pre();

					for (ParseNode const& childNode : blockNode)
					{
						if (childNode.IsType(id_Code))
							html.T(childNode.SrcText());
						else if (childNode.IsType(id_LineEnd))
							html.T("\r\n");
						else
							EnsureThrow(childNode.IsType(id_Junk) || childNode.IsType(Parse::id_HWs));
					}

					html.EndPre();
				}
				else if (blockNode.IsType(id_HorizRule))
				{
					html.Hr();
				}
				else if (blockNode.IsType(id_Link))
				{
					Seq url { blockNode.FlatFindRef(Uri::id_URI_reference).SrcText() };
					if (m_htmlPermitLinks)
						html.P().A(url, url).EndP();
					else
						html.P().T(url).EndP();
				}
				else if (blockNode.IsType(id_Heading))
				{
					     if (blockNode.IsType(id_Heading1)) { html.H1(); ParaElemsToHtml(html, blockNode); html.EndH1(); }
					else if (blockNode.IsType(id_Heading2)) { html.H2(); ParaElemsToHtml(html, blockNode); html.EndH2(); }
					else if (blockNode.IsType(id_Heading3)) { html.H3(); ParaElemsToHtml(html, blockNode); html.EndH3(); }
					else if (blockNode.IsType(id_Heading4)) { html.H4(); ParaElemsToHtml(html, blockNode); html.EndH4(); }
					else EnsureThrow(!"Unrecognized heading type");
				}
				else if (blockNode.IsType(id_Paragraph))
				{
					html.P();
					ParaElemsToHtml(html, blockNode);
					html.EndP();
				}
				else 
					EnsureThrow(blockNode.IsType(id_BlankLine) || blockNode.IsType(id_Junk) || blockNode.IsType(id_ListKind));
			}

			return html;
		}


		HtmlBuilder& Transform::ParaElemsToHtml(HtmlBuilder& html, ParseNode const& containerNode) const
		{
			bool any {};
			bool needSpace {};

			for (ParseNode const& elemNode : containerNode)
			{
				if (!elemNode.IsType(id_Junk))
					if (elemNode.IsType(Parse::id_HWs) || elemNode.IsType(id_LineEnd))
					{
						if (any)
							needSpace = true;
					}
					else
					{
						if (needSpace)
						{
							html.T(" ");
							needSpace = false;
						}

						if (elemNode.IsType(id_Text))
							html.T(elemNode.SrcText());
						else if (elemNode.IsType(id_Code))
							html.Code(elemNode.SrcText());
						else if (elemNode.IsType(id_EscPair))
							html.T(elemNode.SrcText().DropByte());
						else if (elemNode.IsType(Html::id_CharRef))
							html.AddTextUnescaped(elemNode.SrcText());
						else if (elemNode.IsType(id_Bold))
							html.B().Method(*this, &Transform::ParaElemsToHtml, elemNode).EndB();
						else if (elemNode.IsType(id_Italic))
							html.I().Method(*this, &Transform::ParaElemsToHtml, elemNode).EndI();
						else
							EnsureThrow(!"Unexpected paragraph element");

						any = true;
					}
			}

			return html;
		}


		TextBuilder& Transform::ToText(TextBuilder& text, ParseNode const& containerNode) const
		{
			for (ParseNode const& blockNode : containerNode)
			{
				if (blockNode.IsType(id_QuoteBlock))
				{
					text.Scope("> ");
					ToText(text, blockNode);
					text.EndScope();
				}
				else if (blockNode.IsType(id_List))
				{
					bool        isLoose    { blockNode.IsType(id_ListLoose) };
					bool        isOpened   {};
					Ruid const* listKind   {};
					uint64      nextItemNr { 1 };

					for (ParseNode const& childNode : blockNode)
					{
						if (childNode.IsType(id_ListItem))
						{
							if (!isOpened)
							{
								ParseNode const& pnListKind { childNode.FirstChild() };

								listKind = &pnListKind.Type();

								if (pnListKind.IsType(id_ListKindOrdered))
								{
									Seq startNr { pnListKind.FlatFindRef(id_ListItemNr).SrcText() };
									nextItemNr = startNr.ReadNrUInt64Dec();
								}

								isOpened = true;
							}

							if (listKind->Is(id_ListKindBullet))
							{
								char const* prefix {};
								     if (listKind->Is(id_ListKindBulletStar)) prefix = "* ";
								else if (listKind->Is(id_ListKindBulletDash)) prefix = "- ";
								else if (listKind->Is(id_ListKindBulletPlus)) prefix = "+ ";
								else EnsureThrow(!"Unrecognized bullet list kind");

								text.ListItemScope(prefix);
							}
							else
							{
								Str prefix;
								if (nextItemNr < 10)
									prefix.Add(" ");
								prefix.UInt(nextItemNr);
								++nextItemNr;

								     if (listKind->Is(id_ListKindOrderedDot)) prefix.Add(". ");
								else if (listKind->Is(id_ListKindOrderedCBr)) prefix.Add(") ");
								else EnsureThrow(!"Unrecognized ordered list kind");

								text.ListItemScope(prefix);
							}
							
							if (!isLoose)
								ParaElemsToText(text, childNode.FlatFindRef(id_Paragraph));
							else
								ToText(text, childNode);

							text.EndScope();
						}
						else
							EnsureThrow(childNode.IsType(id_BlankLine) || childNode.IsType(id_Junk));
					}

					text.Br();
				}
				else if (blockNode.IsType(id_CodeBlock))
				{
					text.Scope("    ");
					for (ParseNode const& childNode : blockNode)
					{
						if (childNode.IsType(id_Code))
							text.Add(childNode.SrcText());
						else if (childNode.IsType(id_LineEnd))
							text.Add("\r\n");
						else
							EnsureThrow(childNode.IsType(id_Junk) || childNode.IsType(Parse::id_HWs));
					}
					text.EndScope();
					text.Br();
				}
				else if (blockNode.IsType(id_HorizRule))
				{
					sizet hrLen { text.MaxCurLineTextWidth() - 1 };
					text.Add(Str().Chars(hrLen, '-')).Br();
				}
				else if (blockNode.IsType(id_Link))
				{
					Seq url { blockNode.FlatFindRef(Uri::id_URI_reference).SrcText() };
					text.Add(url).Br();
				}
				else if (blockNode.IsType(id_Heading))
				{
					ParaElemsToText(text, blockNode);
					text.Br();
				}
				else if (blockNode.IsType(id_Paragraph))
				{
					ParaElemsToText(text, blockNode);
					text.Br();
				}
				else 
					EnsureThrow(blockNode.IsType(id_BlankLine) || blockNode.IsType(id_Junk) || blockNode.IsType(id_ListKind));
			}

			return text;
		}


		TextBuilder& Transform::ParaElemsToText(TextBuilder& text, ParseNode const& containerNode) const
		{
			bool any {};
			bool needSpace {};

			for (ParseNode const& elemNode : containerNode)
			{
				if (!elemNode.IsType(id_Junk))
					if (elemNode.IsType(Parse::id_HWs) || elemNode.IsType(id_LineEnd))
					{
						if (any)
							needSpace = true;
					}
					else
					{
						if (needSpace)
						{
							text.Add(" ");
							needSpace = false;
						}

						if (elemNode.IsType(id_Text))
							text.Add(elemNode.SrcText());
						else if (elemNode.IsType(id_Code))
							text.Add(elemNode.SrcText());
						else if (elemNode.IsType(id_EscPair))
							text.Add(elemNode.SrcText().DropByte());
						else if (elemNode.IsType(Html::id_CharRefName))
						{
							Html::CharRefNameInfo const* crni { Html::FindCharRefByName(elemNode.SrcText()) };
							if (!crni)
								text.Add(elemNode.SrcText());
							else
							{
								text.Utf8Char(crni->m_codePoint1);
								if (crni->m_codePoint2)
									text.Utf8Char(crni->m_codePoint2);
							}
						}
						else if (elemNode.IsType(Html::id_CharRefDec))
						{
							Seq digits { elemNode.SrcText() };
							EnsureThrow(digits.StripPrefixExact("&#"));

							uint c { digits.ReadNrUInt32Dec() };
							if (!Unicode::IsValidCodePoint(c))
								c = 0xFFFD;

							text.Utf8Char(c);
						}
						else if (elemNode.IsType(Html::id_CharRefHex))
						{
							Seq digits { elemNode.SrcText() };
							EnsureThrow(digits.StripPrefixInsensitive("&#x"));

							uint c { digits.ReadNrUInt32(16) };
							if (!Unicode::IsValidCodePoint(c))
								c = 0xFFFD;

							text.Utf8Char(c);
						}
						else if (elemNode.IsType(id_Bold) || elemNode.IsType(id_Italic))
							ParaElemsToText(text, elemNode);
						else
							EnsureThrow(!"Unexpected paragraph element");

						any = true;
					}
			}

			return text;
		}

	}
}
