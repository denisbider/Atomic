#include "AtIncludes.h"
#include "AtHtmlTransform.h"

#include "AtHtmlRead.h"

namespace At
{
	namespace Html
	{
		HtmlBuilder& Transform::ToEmbeddableHtml(HtmlBuilder& html, EmbedCx& cx)
		{
			bool haveText {};
			bool haveWs {};
			Str exciseEndTagLower;
			Str elemTagLower;
			Str attrTagLower;

			auto onText = [&] () { if (!haveText) haveText = true; else if (haveWs) { html.T(" "); haveWs = false; } };

			for (ParseNode const& node : m_tree.Root())
			{
				if (node.IsType(id_HtmlWs))
				{
					if (!exciseEndTagLower.Any())
						haveWs = true;
				}
				else if (node.IsType(id_Text))
				{
					if (!exciseEndTagLower.Any())
					{
						onText();
						html.T(node.SrcText());
					}
				}
				else if (node.IsType(id_CharRef))
				{
					if (!exciseEndTagLower.Any())
					{
						onText();
						html.AddTextUnescaped(node.SrcText());
					}
				}
				else if (node.IsType(id_Comment))
				{
					// Ignored.
				}
				else if (node.IsType(id_CData))
				{
					if (!exciseEndTagLower.Any())
					{
						ParseNode const& cdataContentNode { node.DeepFindRef(id_CDataContent) };
						html.T(cdataContentNode.SrcText());
					}
				}
				else if (node.IsType(id_SpecialElem))
				{
					// Excise all content for elements like "script" and "style"
					ParseNode const& elemTagNode { node.DeepFindRef(id_Tag) };
					exciseEndTagLower.Clear().Lower(elemTagNode.SrcText());
				}
				else if (node.IsType(id_StartTag))
				{
					ParseNode const& elemTagNode { node.DeepFindRef(id_Tag) };
					elemTagLower.Clear().Lower(elemTagNode.SrcText());

					ElemInfo const* ei = FindElemInfo_ByTagExact(elemTagLower);
					if (ei && ei->m_embedAction == EmbedAction::Allow)
					{
								if (ei->m_type == ElemType::Void    ) html.AddVoidElem    (elemTagLower);
						else if (ei->m_type == ElemType::NonVoid ) html.AddNonVoidElem (elemTagLower);
						else EnsureThrow(!"Unexpected ElemType");

						cx.OnNewElem();

						for (ParseNode const& childNode : node)
							if (childNode.IsType(id_Attr))
							{
								ParseNode const& attrTagNode { node.DeepFindRef(id_AttrName) };
								attrTagLower.Clear().Lower(attrTagNode.SrcText());

								AttrInfo const* ai = ei->FindAttrInfo_ByTagExact(attrTagLower);
								if (ai && ai->m_embedAction == EmbedAction::Allow)
								{
									Seq attrValueOrig;
									if (!ReadAttrValue(childNode, cx.m_store, attrValueOrig))
										html.AddAttr(attrTagLower);
									else
									{
										Seq attrValueSan = ai->m_embedSanitizer(attrValueOrig, cx);
										html.AddAttr(attrTagLower, attrValueSan);
									}
								}
							}

						if (ei->m_finalizer != nullptr)
							ei->m_finalizer(html, cx);
					}
				}
				else if (node.IsType(id_EndTag))
				{
					ParseNode const& elemTagNode { node.DeepFindRef(id_Tag) };
					elemTagLower.Clear().Lower(elemTagNode.SrcText());

					if (exciseEndTagLower.Any())
					{
						if (Seq(elemTagLower).EqualExact(exciseEndTagLower))
							exciseEndTagLower.Clear();
					}
					else
					{
						// If the end tag does not correspond to an open tag, ignore it.
						if (html.m_tags.Contains(elemTagLower))
						{
							// The end tag corresponds to an open tag. Close this tag, as well as any other unclosed tags in between.
							while (html.m_tags.Any())
							{
								Seq tagToClose = html.m_tags.Last();
								bool done = tagToClose.EqualExact(elemTagLower);
								html.EndNonVoidElem(tagToClose);
								if (done)
									break;
							}
						}
					}
				}
				else if (node.IsType(id_TrashTag))
				{
					// Ignored.
				}
				else if (node.IsType(id_SuspectChar))
				{
					if (!exciseEndTagLower.Any())
					{
						html.T(node.SrcText());
					}
				}
				else
					EnsureThrow(!"Unrecognized HTML particle type");
			}

			// Close open tags
			while (html.m_tags.Any())
				html.EndNonVoidElem(html.m_tags.Last());

			return html;
		}


		TextBuilder& Transform::ToText(TextBuilder& text, EmbedCx& /*cx*/)
		{
			// ...
			text.Add("[[[ Implement Html::Transform::ToText ]]]");
			return text;
		}

	}
}
