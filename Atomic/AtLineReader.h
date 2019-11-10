#pragma once

#include "AtException.h"
#include "AtFile.h"
#include "AtStr.h"

namespace At
{

	class LineReader : NoCopy
	{
	public:
		// Lines larger than INT32_MAX characters are not supported by MultiByteToWideChar/WideCharToMultiByte.
		LineReader(Seq content,            sizet maxLineSize = INT32_MAX) :                      m_maxLineSize(maxLineSize), m_remaining(content)        {}
		LineReader(FileLoader const& file, sizet maxLineSize = INT32_MAX) : m_path(file.Path()), m_maxLineSize(maxLineSize), m_remaining(file.Content()) {}

		Seq Path() const { return m_path; }

		// Reads next line. If there is already content in outLine, it is cleared.
		// Returns false if a line cannot be read because end of content has been reached.
		// Considers LF characters to be line breaks. If there is a CR before an LF, it is discarded.
		// Throws InputErr if line is longer than allowed.
		bool ReadLine(Str& outLine);

		sizet LineNr() const { return m_lineNr; }
		bool LastLineEndCr() const { return m_lastLineEndCr; }
		bool LastLineEndLf() const { return m_lastLineEndLf; }
	
	private:
		Str   m_path;
		sizet m_maxLineSize   {};
		Seq   m_remaining;
		sizet m_lineNr        {};
		bool  m_lastLineEndCr {};
		bool  m_lastLineEndLf {};
	};

}
