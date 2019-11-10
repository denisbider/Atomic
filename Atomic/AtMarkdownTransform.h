#pragma once

#include "AtHtmlBuilder.h"
#include "AtMarkdownGrammar.h"
#include "AtTextBuilder.h"

namespace At
{
	namespace Markdown
	{
		class Transform : NoCopy
		{
		public:
			Transform(Seq srcText) : m_tree(srcText) {}

			bool Parse() { return m_tree.Parse(C_Document); }

			ParseTree&       Tree()       { return m_tree; }
			ParseTree const& Tree() const { return m_tree; }

			Transform& SetHtmlPermitLinks(bool permitLinks) { m_htmlPermitLinks = permitLinks; return *this; }
			HtmlBuilder& ToHtml(HtmlBuilder& html) const { return ToHtml(html, m_tree.Root()); }

			TextBuilder& ToText(TextBuilder& text) const { return ToText(text, m_tree.Root()); }

		private:
			ParseTree m_tree;
			bool      m_htmlPermitLinks {};

			HtmlBuilder& ToHtml(HtmlBuilder& html, ParseNode const& containerNode) const;
			HtmlBuilder& ParaElemsToHtml(HtmlBuilder& html, ParseNode const& containerNode) const;

			TextBuilder& ToText(TextBuilder& text, ParseNode const& containerNode) const;
			TextBuilder& ParaElemsToText(TextBuilder& text, ParseNode const& containerNode) const;
		};
	}
}
