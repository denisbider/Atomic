#include "AtIncludes.h"
#include "AtLineReader.h"

#include "AtNumCvt.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	bool LineReader::ReadLine(Str& outLine)
	{
		outLine.Clear();
		++m_lineNr;
	
		if (!m_remaining.n)
			return false;

		Seq origLine = m_remaining.ReadToByte(10, m_maxLineSize);	
		if (!m_remaining.n)
			m_lastLineEndLf = false;
		else
		{
			if (m_remaining.p[0] != 10)
			{
				Str msg;
				msg.Set("Line ").UInt(m_lineNr);
				if (m_path.Any())
					msg.Add(" in ").Add(m_path);
				msg.Add(" exceeds ").UInt(m_maxLineSize).Add(" bytes");
				throw InputErr(msg);
			}
			
			m_remaining.DropByte();
			m_lastLineEndLf = true;
		}

		if (!origLine.n || origLine.p[origLine.n-1] != 13)
			m_lastLineEndCr = false;
		else
		{
			--origLine.n;
			m_lastLineEndCr = true;
		}
	
		outLine.Set(origLine);
		return true;
	}

}
