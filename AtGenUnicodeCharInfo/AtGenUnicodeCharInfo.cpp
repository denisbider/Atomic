// Changes 2020-07-22, by db:
//
// - This code was originally written in 2015, largely inspired by: https://github.com/JuliaStrings/utf8proc/blob/master/data/charwidths.jl
//   At the time, it used Unifont as one of the main sources of character widths: https://unifoundry.com/unifont.html
//   Relevant comment in the current version of charwidths.jl:
//   "We used to also use data from GNU Unifont, but that has proven unreliable and unlikely to match widths assumed by terminals."
//   This is confirmed in my experience (e.g. Bitvise case 44368), therefore I'm following Julia and removing Unifont as a source input.
//
// - Removed the following Unicode categories from those initialized to zero-width:
//   Symbol_Modifier: "These are not combining diacriticals, but are 'spacing characters' [...] They are what you use if you want a bare
//       diaresis or acute" https://github.com/JuliaStrings/utf8proc/issues/167
//   Other_NotAssigned, Other_PrivateUse, Other_Surrogate: "if they are printed as the replacement character U+FFFD they will have width 1"
//
// - Arabic characters U+0601, U+0602, U+0603 and U+06DD are no longer exceptions and are set to zero-width as per category Other_Format.
//   From charwidths.jl: "some of these, like U+0601, can have a width in some cases but normally act like prepended combining marks"

#include "AtConsole.h"
#include "AtCsv.h"
#include "AtFile.h"
#include "AtPath.h"
#include "AtUnicode.h"
#include "AtWinErr.h"
#include "AtWinStr.h"

using namespace At;


void FormatCodePoint(Str& s, uint codePoint)
{
	s.UInt(codePoint).Add(" (U+").UInt(codePoint, 16, 4).Add(")");
}


class CharInfoBuilder
{
public:
	CharInfoBuilder(Seq dir) : m_dir(dir) {}

	void Run();

private:
	struct CharEntry
	{
		Unicode::CharInfo m_ci;
		bool              m_haveCategory       {};
		bool              m_haveEastAsianWidth {};
	};
	
	Str            m_dir;
	Vec<CharEntry> m_chars;

	void ReadCharCategories  (Seq fileName);
	void SetDefaultWidths    ();
	void ReadEastAsianWidths (Seq fileName);
	void Sanitize            ();
	void WriteCharInfo       (Seq fileName);
	void CompareToBuiltIn    ();
};


void CharInfoBuilder::Run()
{
	m_chars.Clear();
	m_chars.ResizeExact(Unicode::MaxAssignedCodePoint + 1);

	Str unicodeDataFileName    { JoinPath(m_dir, "UnicodeData.txt"       ) };
	Str eastAsianWidthFileName { JoinPath(m_dir, "EastAsianWidth.txt"    ) };
	Str outputFileName         { JoinPath(m_dir, "AtUnicodeCharInfo.cpp" ) };

	ReadCharCategories  (unicodeDataFileName);
	SetDefaultWidths    ();
	ReadEastAsianWidths (eastAsianWidthFileName);
	Sanitize            ();

	WriteCharInfo       (outputFileName);
	CompareToBuiltIn    ();
}


void CharInfoBuilder::ReadCharCategories(Seq fileName)
{
	Console::Out(Str("\r\n"
				 "Reading Unicode character categories from ").Add(fileName).Add("\r\n"));

	sizet nrProcessed {};
	sizet nrRanges    {};
	sizet nrErrors    {};
	sizet nrIgnored   {};

	FileLoader fileLoader { fileName };
	CsvReader  csvReader  { fileLoader, ';', '#' };
	Vec<Str>   record;
	sizet      nrFields;
	uint       firstCodePointInRange {};

	auto onError_noCodePoint = [&] (Seq msg)
		{ ++nrErrors; Console::Out(Str("Error at line ").UInt(csvReader.LineNr()).Add(": ").Add(msg).Add("\r\n")); };

	auto onError = [&] (uint codePoint, Seq msg)
		{ ++nrErrors; Console::Out(Str("Error at line ").UInt(csvReader.LineNr()).Add(", code point ").Fun(FormatCodePoint, codePoint).Add(": ").Add(msg).Add("\r\n")); };

	while (csvReader.ReadRecord(record, nrFields))
	{
		if (nrFields >= 3)
		{
			Seq codePointReader { record[0] };
			uint codePoint      { codePointReader.ReadNrUInt32(16) };

			if (codePointReader.n)
				onError_noCodePoint(Str("Unrecognized code point").Add(record[0]));
			else if (codePoint >= m_chars.Len())
				++nrIgnored;
			else
			{
				Seq catCode = record[2];
				uint cat { Unicode::Category::CodeToValue(catCode.p, catCode.n) };

				if (cat == Unicode::Category::Invalid || cat == Unicode::Category::Unknown)
					onError(codePoint, Str("Unrecognized code point category \"").Add(record[2]).Add("\""));
				else
				{
					auto setCat = [&] (uint codePoint)
						{
							CharEntry& entry { m_chars[codePoint] };
							if (entry.m_haveCategory)
								onError(codePoint, "Duplicate code point");
							else
							{
								++nrProcessed;
								entry.m_ci.m_cat = cat;
								entry.m_haveCategory = true;
							}
						};

					setCat(codePoint);

					bool isFirstInRange { Seq(record[1]).EndsWithExact(", First>") };
					bool isLastInRange  { Seq(record[1]).EndsWithExact(", Last>") };

					if (isFirstInRange)
					{
						if (!firstCodePointInRange)
							firstCodePointInRange = codePoint;
						else
							onError(codePoint, "First code point in range already set");
					}
					else
					{
						if (!isLastInRange)
						{
							if (firstCodePointInRange != 0)
								onError(codePoint, "First code point in range not followed by last");
						}
						else
						{
							if (firstCodePointInRange == 0)
								onError(codePoint, "Last code point in range appears without first");
							else if (codePoint <= firstCodePointInRange)
								onError(codePoint, "Invalid last code point in range");
							else
							{
								++nrRanges;
								for (uint i=firstCodePointInRange+1; i!=codePoint; ++i)
									setCat(i);
							}
						}

						firstCodePointInRange = 0;
					}
				}
			}
		}
	}

	Console::Out(Str().UInt(nrProcessed).Add(" processed, ").UInt(nrRanges).Add(" ranges, ").UInt(nrErrors).Add(" errors, ").UInt(nrIgnored).Add(" ignored\r\n"));
}


void CharInfoBuilder::SetDefaultWidths()
{
	sizet nrZero {};
	sizet nrOne  {};

	for (CharEntry& entry : m_chars)
		switch (entry.m_ci.m_cat)
		{
		case Unicode::Category::Other_Control:
		case Unicode::Category::Other_Format:
		case Unicode::Category::Mark_SpacingCombining:
		case Unicode::Category::Mark_Enclosing:
		case Unicode::Category::Mark_Nonspacing:
		case Unicode::Category::Separator_Line:
		case Unicode::Category::Separator_Paragraph:
		case Unicode::Category::Separator_Space:
			++nrZero;
			entry.m_ci.m_width = 0;
			break;

		default:
			++nrOne;
			entry.m_ci.m_width = 1;
			break;
		}

	Console::Out(Str("\r\n")
				.UInt(nrZero).Add(" default widths set to 0, ").UInt(nrOne).Add(" set to 1\r\n"));
}


void CharInfoBuilder::ReadEastAsianWidths(Seq fileName)
{
	Console::Out(Str("\r\n"
					 "Reading East Asian widths from ").Add(fileName).Add("\r\n"));

	sizet nrProcessed {};
	sizet nrNarrow    {};
	sizet nrWide      {};
	sizet nrErrors    {};
	sizet nrNoCat     {};
	sizet nrIgnored   {};
	sizet nrChanged   {};

	FileLoader fileLoader { fileName };
	CsvReader  csvReader  { fileLoader, ';', '#' };
	Vec<Str>   record;
	sizet      nrFields;

	auto onError = [&] (uint codePoint, Seq msg)
		{ ++nrErrors; Console::Out(Str("Error at line ").UInt(csvReader.LineNr()).Add(", code point ").Fun(FormatCodePoint, codePoint).Add(": ").Add(msg).Add("\r\n")); };

	while (csvReader.ReadRecord(record, nrFields))
	{
		if (nrFields >= 2)
		{
			Seq  firstField     { record[0] };
			uint firstCodePoint { firstField.ReadNrUInt32(16) };
			uint lastCodePoint  { firstCodePoint };

			if (firstField.StripPrefixExact(".."))
				lastCodePoint = firstField.ReadNrUInt32(16);

			uint w           { UINT_MAX };
			Seq  secondField { Seq(record[1]).Trim() };
			
			     if (secondField == "H" || secondField == "Na") w = 1;
			else if (secondField == "F" || secondField == "W" ) w = 2;
			else if (secondField != "N" && secondField != "A" )
				onError(firstCodePoint, Str("Unrecognized East_Asian_Width property value \"").Add(secondField).Add("\""));

			if (w != UINT_MAX)
			{
				for (uint codePoint=firstCodePoint; codePoint<=lastCodePoint; ++codePoint)
				{
					if (codePoint >= m_chars.Len())
						++nrIgnored;
					else
					{
						CharEntry& entry { m_chars[codePoint] };

						if (!entry.m_haveCategory)
							++nrNoCat;

						if (entry.m_haveEastAsianWidth)
						{
							if (w != entry.m_ci.m_width)
								onError(codePoint, Str("Duplicate code point with conflicting width: ").UInt(w).Add(" instead of ").UInt(entry.m_ci.m_width));
						}
						else
						{
							if (w == 1)
								++nrNarrow;
							else if (w == 2)
								++nrWide;

							if (w != entry.m_ci.m_width)
								++nrChanged;

							++nrProcessed;
							entry.m_ci.m_width = w;
							entry.m_haveEastAsianWidth = true;
						}
					}
				}
			}
		}
	}

	Console::Out(Str().UInt(nrProcessed).Add(" processed, ").UInt(nrNarrow).Add(" narrow, ").UInt(nrWide).Add(" wide, ").UInt(nrErrors).Add(" errors, ")
				.UInt(nrNoCat).Add(" without category, ").UInt(nrIgnored).Add(" ignored, ").UInt(nrChanged).Add(" changed\r\n"));
}


void CharInfoBuilder::Sanitize()
{
	bool anySanitized {};
	auto sanitize = [&anySanitized] (uint codePoint, Unicode::CharInfo& ci, uint width)
		{
			if (ci.m_width != width)
			{
				Console::Out(Str("Sanitize: ").UInt(codePoint).Add(" U+").UInt(codePoint, 16, 4).Add(" ").Add(Unicode::Category::ValueToName(ci.m_cat))
					.Add(" changed from width ").UInt(ci.m_width).Add(" to ").UInt(width).Add("\r\n"));

				ci.m_width = width;
				anySanitized = true;
			}
		};

	Console::Out("\r\n");

	for (uint codePoint=0; codePoint!=m_chars.Len(); ++codePoint)
	{
		CharEntry& entry { m_chars[codePoint] };

		if (entry.m_ci.m_cat == Unicode::Category::Mark_Nonspacing ||
		    entry.m_ci.m_cat == Unicode::Category::Other_Control ||
			entry.m_ci.m_cat == Unicode::Category::Other_Format)
		{
			sanitize(codePoint, entry.m_ci, 0);
		}
		else if (entry.m_ci.m_cat == Unicode::Category::Other_PrivateUse ||
				 entry.m_ci.m_cat == Unicode::Category::Other_NotAssigned)
		{
			sanitize(codePoint, entry.m_ci, 1);
		}
	}

	if (!anySanitized)
		Console::Out("No character widths found to sanitize\r\n");
}


void CharInfoBuilder::WriteCharInfo(Seq fileName)
{
	Console::Out(Str("\r\n"
					 "Writing character information to ").Add(fileName).Add("\r\n"));

	Str text;
	text.ReserveExact(m_chars.Len() * 5);

	text.Set("#include \"AtIncludes.h\"\r\n"
			 "#include \"AtUnicode.h\"\r\n"
			 "\r\n"
			 "namespace At\r\n"
			 "{\r\n"
			 "\tchar const c_unicodeCharInfo[UnicodeCharInfo::TotalParts][8193] {\r\n");

	uint codePoint {};
	for (uint plane=0; plane!=3; ++plane)
		for (uint planePart=0; planePart!=8; ++planePart)
			for (uint partLine=0; partLine!=256; ++partLine)
			{
				EnsureThrow(codePoint == (plane * 0x10000) + (planePart * 0x2000) + (partLine * 32));
				text.Add("\t/*").UInt(codePoint, 16, 5).Add("*/ \"");
				for (uint i=0; i!=32; ++i)
				{
					CharEntry const& entry { m_chars[codePoint] };
					uint v { entry.m_ci.m_cat | entry.m_ci.m_width };
					EnsureThrow(v == entry.m_ci.m_cat + entry.m_ci.m_width);

					text.Add("\\x").UInt(v, 16, 2);
					++codePoint;
				}

				if (partLine < 255)
					text.Add("\"\r\n");
				else if (planePart < 7 || plane < 2)
					text.Add("\",\r\n\r\n");
				else
					text.Add("\"\r\n");
			}

	text.Add("\t};\r\n"
			 "}\r\n");

	HANDLE hFile { CreateFileW(WinStr(fileName).Z(), GENERIC_WRITE, FILE_SHARE_DELETE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0) };
	if (hFile == INVALID_HANDLE_VALUE)
		Console::Out(Str("Error creating file ").Add(fileName).Add(": ").Fun(DescribeWinErr, GetLastError()).Add("\r\n"));
	else
	{
		OnExit closeFile( [&] () { CloseHandle(hFile); } );

		DWORD bytesWritten;
		if (!WriteFile(hFile, text.Ptr(), NumCast<DWORD>(text.Len()), &bytesWritten, 0))
			Console::Out(Str("Error writing to file ").Add(fileName).Add(": ").Fun(DescribeWinErr, GetLastError()).Add("\r\n"));
		else if (bytesWritten != text.Len())
			Console::Out(Str("Error writing to file ").Add(fileName).Add(": unexpected number of bytes written: ").UInt(bytesWritten).Add(" instead of ").UInt(text.Len()).Add("\r\n"));
		else
			Console::Out("Done.\r\n");
	}
}


void CharInfoBuilder::CompareToBuiltIn()
{
	Console::Out("\r\n"
				"Comparison to built-in info:\r\n");

	sizet nrChangedBoth  {};
	sizet nrChangedCat   {};
	sizet nrChangedWidth {};
	sizet nrSame         {};
	sizet changeMatrix[3][3] = {};

	for (uint codePoint=0; codePoint!=m_chars.Len(); ++codePoint)
	{
		CharEntry const& entry { m_chars[codePoint] };
		Unicode::CharInfo ci { Unicode::CharInfo::Get(codePoint) };

		if (entry.m_ci.m_cat != ci.m_cat && entry.m_ci.m_width != ci.m_width)
			++nrChangedBoth;
		else if (entry.m_ci.m_cat != ci.m_cat)
			++nrChangedCat;
		else if (entry.m_ci.m_width != ci.m_width)
			++nrChangedWidth;
		else
			++nrSame;

		EnsureThrow(ci.m_width <= 2);
		EnsureThrow(entry.m_ci.m_width <= 2);
		++(changeMatrix[ci.m_width][entry.m_ci.m_width]);
	}

	Str msg;
	msg.UInt(nrChangedBoth).Add(" change category and width, ").UInt(nrChangedCat).Add(" change category only, ")
				.UInt(nrChangedWidth).Add(" change width only, ").UInt(nrSame).Add(" stay the same\r\n"
				"\r\n"
				"           To 0:   To 1:   To 2:\r\n");

	for (uint fromWidth=0; fromWidth!=3; ++fromWidth)
	{
		msg.Add("From ").UInt(fromWidth).Add(": ");
		for (uint toWidth=0; toWidth!=3; ++toWidth)
			msg.UInt(changeMatrix[fromWidth][toWidth], 10, 0, CharCase::Upper, 8);
		msg.Add("\r\n");
	}

	Console::Out(msg);
}


int main(int argc, char** argv)
{
	if (argc != 2)
	{
		Console::Out("\r\n"
					 "\r\n"
					 "Usage: AtGenUnicodeCharInfo <dataDirectory>\r\n"
					 "\r\n"
					 "Generates Unicode character category and monospace display width information\r\n"
					 "for inclusion in the Atomic library.\r\n"
					 "\r\n"
					 "'dataDirectory' must contain these files from the Unicode Character Database:\r\n"
					 "\r\n"
					 "  UnicodeData.txt\r\n"
					 "  EastAsianWidth.txt\r\n"
					 "\r\n"
					 "  https://www.unicode.org/Public/UCD/latest/ucd/ \r\n"
					 "\r\n");

		return 2;
	}

	try
	{
		CharInfoBuilder(argv[1]).Run();
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("AtGenUnicodeCharInfo terminated by exception:\r\n")
					.Add(e.what()).Add("\r\n"));
		return 1;
	}
}
