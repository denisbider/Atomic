#include "AtIncludes.h"
#include "AtImfMsgWriter.h"


namespace At
{
	namespace Imf
	{

		// MsgWriter

		MsgWriter::MsgWriter(Str& s, sizet maxLineLen) : m_s(s), m_maxLineLen(maxLineLen)
		{
			m_curLineLen = Seq(m_s).RevReadToByte(10).n;
		}

		
		void MsgWriter::CloseSection()
		{
			EnsureThrow(m_sectionDepth > 0);
			if (!--m_sectionDepth)
			{
				Add(m_section);
				m_section.Clear();
			}
		}


		MsgWriter& MsgWriter::Add(Seq text)
		{
			if (text.n)
			{
				if (m_sectionDepth > 0)
				{
					// We are currently inside a section, postpone adding this text
					m_section.Add(text);
				}
				else
				{
					Seq firstLine { text.ReadToByte(13) };
					if (firstLine.n)
					{
						if (m_curLineLen + firstLine.n <= m_maxLineLen)
						{
							// First line of text fits into current line of output
							m_s.Add(firstLine);
							m_curLineLen += firstLine.n;
						}
						else
						{
							// First line of text does not fit into current line of output. Begin a new line
							while (m_s.Any() && (m_s.Last() == ' ' || m_s.Last() == '\t'))
								m_s.PopLast();
							
							if (firstLine.p[0] == ' ' || firstLine.p[0] == '\t')
								m_s.Add("\r\n");
							else
							{
								m_s.Add("\r\n ");
								++m_curLineLen;
							}

							m_s.Add(firstLine);
							m_curLineLen = firstLine.n;
						}
					}

					// Append subsequent input lines, if any
					if (text.n)
					{
						EnsureThrow(text.StartsWithExact("\r\n"));
						m_s.Add(text);
						m_curLineLen = Seq(text).RevReadToByte(10).n;
					}
				}
			}

			return *this;
		}

	}
}
