#pragma once

#include "AtHtmlBuilder.h"
#include "AtHtmlEmbed.h"
#include "AtHtmlGrammar.h"
#include "AtTextBuilder.h"


namespace At
{
	namespace Html
	{
		class Transform : NoCopy
		{
		public:
			Transform(Seq srcText) : m_tree(srcText) {}

			bool Parse() { return m_tree.Parse(C_Document); }

			ParseTree&       Tree()       { return m_tree; }
			ParseTree const& Tree() const { return m_tree; }

			HtmlBuilder& ToEmbeddableHtml(HtmlBuilder& html, EmbedCx& cx);
			TextBuilder& ToText(TextBuilder& text, EmbedCx& cx);

		private:
			ParseTree m_tree;
		};
	}
}
