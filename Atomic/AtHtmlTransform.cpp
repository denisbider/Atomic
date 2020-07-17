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
			Str elemTagLower;
			Str attrTagLower;

			auto onText = [&] () { if (!haveText) haveText = true; else if (haveWs) { html.T(" "); haveWs = false; } };

			struct TraverseEntry
			{
				ParseNode::ChildIt m_begin;
				ParseNode::ChildIt m_next;
				ParseNode::ChildIt m_end;

				TraverseEntry(ParseNode const& node) : m_begin(node.begin()), m_next(node.begin()), m_end(node.end()) {}
			};

			Vec<TraverseEntry> traverseEntries;
			traverseEntries.Add(m_tree.Root());

			while (traverseEntries.Any())
			{
				TraverseEntry& te = traverseEntries.Last();
				if (te.m_next == te.m_end)
				{
					traverseEntries.PopLast();
					continue;
				}

				ParseNode const& node = *(te.m_next);
				++(te.m_next);

				if (node.IsType(id_HtmlWs))
				{
					haveWs = true;
				}
				else if (node.IsType(id_Text) || node.IsType(id_RawText))
				{
					onText();
					html.T(node.SrcText());
				}
				else if (node.IsType(id_CharRef))
				{
					onText();
					html.AddTextUnescaped(node.SrcText());
				}
				else if (node.IsType(id_Comment))
				{
					// Ignored
				}
				else if (node.IsType(id_CData))
				{
					ParseNode const& cdataContentNode { node.DeepFindRef(id_CDataContent) };
					html.T(cdataContentNode.SrcText());
				}
				else if (node.IsType(id_RawTextElem))
				{
					// These are "script" and "style" elements. Ignore all content
				}
				else if (node.IsType(id_RcdataElem))
				{
					// These are "title" and "textarea" elements. Process normally
					traverseEntries.Add(node);
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
						else
						{
							Str msg; msg.Add("Unrecognized ElemType: ").Obj(elemTagNode, ParseNode::TagRowCol).Ch(0);
							EnsureFailWithDesc(OnFail::Throw, msg.CharPtr(), __FUNCTION__, __LINE__);
						}

						cx.OnNewElem();

						for (ParseNode const& childNode : node)
							if (childNode.IsType(id_Attr))
							{
								ParseNode const& attrTagNode { childNode.DeepFindRef(id_AttrName) };
								attrTagLower.Clear().Lower(attrTagNode.SrcText());

								AttrInfo const* ai = ei->FindAttrInfo_ByTagExact(attrTagLower);
								if (ai && ai->m_embedAction == EmbedAction::Allow)
								{
									Seq attrValueOrig;
									if (!ReadAttrValue(childNode, attrValueOrig))
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
				else if (node.IsType(id_TrashTag))
				{
					// Ignored.
				}
				else if (node.IsType(id_SuspectChar))
				{
					html.T(node.SrcText());
				}
				else
				{
					Str msg; msg.Add("Unrecognized HTML particle type: ").Obj(node, ParseNode::TagRowCol).Ch(0);
					EnsureFailWithDesc(OnFail::Throw, msg.CharPtr(), __FUNCTION__, __LINE__);
				}
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
