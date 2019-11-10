#pragma once

#include "AtStr.h"

namespace At
{
	class TextBuilder
	{
	public:
		TextBuilder(sizet expectedSize = 8000) { m_s.ReserveExact(expectedSize); }

		TextBuilder& SetMaxWidth(sizet maxWidth) { m_maxWidth = PickMax<sizet>(maxWidth, 39); m_minWidth = m_maxWidth/2; return *this; }
		TextBuilder& SetTabStop(sizet tabStop) { m_tabStop = PickMax<sizet>(tabStop, 1); return *this; }

		Seq Final() const { EnsureThrow(!m_scopes.Any()); return m_s; }

		TextBuilder& Scope(Seq prefix) { Scope(prefix, ScopeType::Generic); return *this; }
		TextBuilder& ListItemScope(Seq prefix) { Scope(prefix, ScopeType::ListItem); return *this; }
		TextBuilder& EndScope();

		sizet ScopesWidth() const { return m_scopesWidth; }
		sizet MaxCurLineTextWidth() const { return PickMax<sizet>(m_minWidth, SatSub(m_maxWidth, m_scopesWidth)); }

		TextBuilder& Add(Seq s);
		TextBuilder& Utf8Char(uint c);

		TextBuilder& Br() { EndLine(); m_br = true; return *this; }

	private:
		struct ScopeType { enum E { Generic, ListItem }; };

		struct ScopeInfo
		{
			ScopeInfo(Seq prefix, sizet widthOffset, sizet width, ScopeType::E type)
				: m_prefix(prefix), m_widthOffset(widthOffset), m_width(width), m_type(type) {}

			Str          m_prefix;
			sizet        m_widthOffset;
			sizet        m_width;
			ScopeType::E m_type;
		};

		sizet          m_maxWidth         { 75 };
		sizet          m_minWidth         { 37 };
		sizet          m_tabStop          {  4 };
		Str            m_s;
		sizet          m_scopesWidth      {};
		Vec<ScopeInfo> m_scopes;
		bool           m_br               {};
		sizet          m_curLineTextWidth {};

		void Scope(Seq prefix, ScopeType::E type);

		bool InLine() const { return m_s.Any() && m_s.Last() != 10; }
		void NewLine() { if (m_s.Any() && m_s.Last() == 13) m_s.Add("\n"); else m_s.Add("\r\n"); m_curLineTextWidth = 0; }
		void EndLine() { if (InLine()) NewLine(); }
		void CheckBr() { if (m_br) { EndLine(); AddPrefixes(); NewLine(); m_br = false; } }
		void AddPrefixes();
		void AddToCurLine(Seq s, sizet width) { m_s.Add(s); m_curLineTextWidth += width; }
	};
}
