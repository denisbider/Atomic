#pragma once

#include "AtLineReader.h"
#include "AtVec.h"


namespace At
{

	// Reads CSV input in RFC 4180 format, with additional support for:
	// - a different field delimiter (other than comma)
	// - comments

	class CsvReader : NoCopy
	{
	public:
		// Specify commentDelim == 0 if no comments are supported by the input format.
		CsvReader(Seq content,            char fieldDelim, char commentDelim) : m_lineReader(content) { Init(fieldDelim, commentDelim); }
		CsvReader(FileLoader const& file, char fieldDelim, char commentDelim) : m_lineReader(file)    { Init(fieldDelim, commentDelim); }

		void SkipLines(sizet nrLines);

		Seq   Path   () const { return m_lineReader.Path   (); }
		sizet LineNr () const { return m_lineReader.LineNr (); }

		// Parses the next CSV record in the file. Skips lines with no fields (blank or comment only).
		// Returns false if no record is available because end of line has been reached.
		// Does not remove elements from the vector passed as argument;
		// only adds elements if there are not enough, or reuses existing ones.
		// Throws InputErr if line could not be parsed.
		bool ReadRecord(Vec<Str>& record, sizet& nrFields);

	private:
		LineReader m_lineReader;
		char       m_fieldDelim;
		char       m_commentDelim;
		char       m_delims[4];

		void Init(char fieldDelim, char commentDelim);

		// Returns false if line ends with incomplete quoted string.
		// If false is returned, ParseField should be called again with the next line,
		// but setting resume=true, and passing the same field of the same output record.
		bool ParseField(bool resume, Str& outField, Seq& remaining);
	};

}
