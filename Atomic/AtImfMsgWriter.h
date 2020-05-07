#pragma once

#include "AtImfGrammar.h"
#include "AtReconstruct.h"


namespace At
{
	namespace Imf
	{

		// MsgWriter

		class MsgWriter : public AuxStorage
		{
		public:
			MsgWriter(Str& s, sizet maxLineLen = 78);		// Can pass maxLineLen == SIZE_MAX for no limit

			Str const& GetStr() const { return m_s; }

			// BeginSection() begins a section in which portions cannot be separated with folding whitespace.
			// CloseSection() ends such a section. Multiple calls can be nested, and must be balanced out.
			void BeginSection()                    { ++m_sectionDepth; }
			void BeginSection(sizet bytesExpected) { ++m_sectionDepth; m_section.ReserveAtLeast(m_section.Len() + bytesExpected); }
			void CloseSection();

			// Adds a portion of text which can normally be separated from preceding text with folding whitespace.
			// If we are currently within a section that is preventing whitespace insertion, the text is instead
			// added to the section, and later added to the message when the section is closed.
			MsgWriter& Add(Seq text);
			MsgWriter& AddVerbatim(Seq text) { EnsureThrow(!m_sectionDepth); m_s.Add(text); return *this; }

		private:
			Str&        m_s;
			sizet const m_maxLineLen;

			sizet       m_curLineLen;
			Str         m_section;
			sizet       m_sectionDepth {};
		};


		class MsgSectionScope : NoCopy
		{
		public:
			MsgSectionScope(MsgWriter& writer)                      : m_writer(writer) { m_writer.BeginSection(); }
			MsgSectionScope(MsgWriter& writer, sizet bytesExpected) : m_writer(writer) { m_writer.BeginSection(bytesExpected); }
			~MsgSectionScope() { m_writer.CloseSection(); }

		private:
			MsgWriter& m_writer;
		};

	}
}
