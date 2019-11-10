#include "AtIncludes.h"
#include "AtCsv.h"

#include "AtNumCvt.h"


namespace At
{

	void CsvReader::Init(char fieldDelim, char commentDelim)
	{
		m_fieldDelim = fieldDelim;
		m_commentDelim = commentDelim;

		sizet i {};
		m_delims     [i++] = m_fieldDelim;
		if (commentDelim != 0)
			m_delims [i++] = m_commentDelim;
		m_delims     [i++] = '"';
		m_delims     [i++] = 0;
	}


	void CsvReader::SkipLines(sizet nrLines)
	{	
		Str discarded;
		while (nrLines-- > 0)
			m_lineReader.ReadLine(discarded);
	}


	bool CsvReader::ReadRecord(Vec<Str>& record, sizet& nrFields)
	{
		nrFields = 0;

		do
		{
			Str line;
			if (!m_lineReader.ReadLine(line))
				return false;
	
			Seq remaining { line };	
			while (true)
			{
				if (!remaining.n)
					break;
	
				if (record.Len() <= nrFields)
					record.Add();
		
				bool resume = false;
				while (true)
				{
					Str& field { record[nrFields] };
					if (!ParseField(resume, field, remaining))
					{
						if (m_lineReader.ReadLine(line))
						{
							remaining = line;
							resume = true;
							continue;
						}
					}
					break;
				}
		
				++nrFields;
			}
		}
		while (nrFields == 0);

		// Clear any columns not present in this record	
		for (sizet i=nrFields; i!=record.Len(); ++i)
			record[i].Clear();

		return true;
	}


	bool CsvReader::ParseField(bool resume, Str& outField, Seq& remaining)
	{
		if (!resume)
		{
			// Begin reading field
			outField.Clear();
		
			Seq chunk { remaining.ReadToFirstByteOf(m_delims) };
			if (!remaining.n)
			{
				// Field not quoted and has no following field
				outField.Add(chunk);
				return true;
			}
		
			if (remaining.p[0] == m_fieldDelim)
			{
				// Field not quoted, but does have a following field
				outField.Add(chunk);
				remaining.DropByte();
				return true;
			}

			if (m_commentDelim != 0 && remaining.p[0] == m_commentDelim)
			{
				// Field not quoted and followed by comment
				outField.Add(chunk);
				remaining.DropBytes(remaining.n);
				return true;
			}
		
			// Field is quoted
			remaining.DropByte();
		}

		// Finish reading quoted field
		while (true)
		{
			Seq chunk { remaining.ReadToByte('"') };
			outField.Add(chunk);
			if (!remaining.n)
			{
				// Field continues on the next line
				if (m_lineReader.LastLineEndCr()) outField.Ch('\r');
				if (m_lineReader.LastLineEndLf()) outField.Ch('\n');
				return false;
			}
		
			remaining.DropByte();
			if (!remaining.n)
			{
				// Quoted field ended, and has no following field
				return true;
			}
		
			if (remaining.p[0] == '"')
			{
				// Quote char is escaped, is part of field
				outField.Ch('"');
				remaining.DropByte();
				continue;
			}

			// Quote char not escaped, expect next field
			remaining.DropToFirstByteNotOf(" \t");
			if (!remaining.n)
			{
				// Quoted field ended, and has no following field
				return true;
			}
		
			if (remaining.p[0] == m_fieldDelim)
			{
				// Quoted field ended, and does have a following field
				remaining.DropByte();
				return true;
			}

			if (m_commentDelim != 0 && remaining.p[0] == m_commentDelim)
			{
				// Quoted field ended and followed by comment
				remaining.DropBytes(remaining.n);
				return true;
			}

			Str msg;
			msg.Set("Expecting either field delimiter or new line at line ").UInt(m_lineReader.LineNr());
			if (Path().Any())
				msg.Add(" in CSV file ").Add(Path());
			throw InputErr(msg);
		}
	}

}
