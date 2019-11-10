#include "AtIncludes.h"
#include "AtTextBuilder.h"

#include "AtUtf8.h"

namespace At
{
	void TextBuilder::Scope(Seq prefix, ScopeType::E type)
	{
		CheckBr();
		EndLine();

		sizet prefixWidth { prefix.CalcWidth() };
		m_scopes.Add(prefix, m_scopesWidth, prefixWidth, type);
		m_scopesWidth += prefixWidth;
	}

	TextBuilder& TextBuilder::EndScope()
	{
		EndLine();
		EnsureThrow(m_scopes.Last().m_widthOffset + m_scopes.Last().m_width == m_scopesWidth);
		m_scopesWidth -= m_scopes.Last().m_width;
		m_scopes.PopLast();
		return *this;
	}

	TextBuilder& TextBuilder::Add(Seq s)
	{
		while (s.n)
		{
			Seq chunk { s.ReadToByte(10) };

			CheckBr();
			if (!InLine())
				AddPrefixes();

			// This is a very lame type of word wrap that will work correctly only when:
			// - only ASCII characters are being encoded, and
			// - the Tab character is not being used.
			//
			// Results will be off when Tab or Unicode are being used.
			//
			// Unfortunately, fixing this is complex. Tabs can't always work correctly unless Unicode is also handled correctly;
			// and Unicode is difficult to handle correctly because we can't actually KNOW, but can only GUESS, how many character
			// positions an unspecified renderer is going to use for a particular code point. Windows does not appear to have a
			// maintained wcwidth equivalent. It seems the most promising way to solve this would be to build a database of
			// Unicode character widths at build time using TextRenderer.MeasureText, or using Graphics.MeasureString. Results
			// would be fully valid only for the font used to measure, but it seems likely the results would be consistent with
			// monospace fonts on other platforms for a majority of code points. This is probably the best that can be done.
			// Users wishing better results would simply have to view emails in HTML. But this is what most users already do;
			// so the case for implementing plaintext tab/wrap better than the below is not compelling.

			sizet maxTextWidth { MaxCurLineTextWidth() };
			while (chunk.n)
			{
				Seq word { chunk.ReadToFirstByteOf(" \t") };
				if (word.n)
				{
					sizet wordWidth { word.CalcWidth(m_scopesWidth + m_curLineTextWidth, m_tabStop) };
					if (m_curLineTextWidth + wordWidth > maxTextWidth &&
						m_curLineTextWidth >= m_minWidth)
					{
						NewLine();
						AddPrefixes();
					}

					AddToCurLine(word, wordWidth);
				}

				Seq ws { chunk.ReadToFirstByteNotOf(" \t") };
				if (ws.n)
				{
					sizet wsWidth { ws.CalcWidth(m_scopesWidth + m_curLineTextWidth, m_tabStop) };
					if (m_curLineTextWidth + wsWidth < maxTextWidth)
						AddToCurLine(ws, wsWidth);
					else
					{
						NewLine();
						AddPrefixes();
					}
				}
			}

			if (s.ReadByte() == 10)
				NewLine();
		}
		return *this;
	}

	TextBuilder& TextBuilder::Utf8Char(uint c)
	{
		byte buf[10];
		uint len { Utf8::WriteCodePoint(buf, c) };
		Add(Seq(buf, len));
		return *this;
	}

	void TextBuilder::AddPrefixes()
	{
		for (ScopeInfo& si : m_scopes)
		{
			m_s.Add(si.m_prefix);

			if (si.m_type == ScopeType::ListItem)
			{
				si.m_prefix.Clear().Chars(si.m_width, ' ');
				si.m_type = ScopeType::Generic;
			}
		}

	}

}
