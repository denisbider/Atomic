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
		bool              m_haveUnifontWidth   {};
		bool              m_haveEastAsianWidth {};
	};
	
	Str            m_dir;
	Vec<CharEntry> m_chars;

	void ReadCharCategories  (Seq fileName);
	void SetDefaultWidths    ();
	void ReadUnifontWidths   (Seq fileName);
	void ReadEastAsianWidths (Seq fileName);
	void Sanitize            ();
	void WriteCharInfo       (Seq fileName);
	void CompareToBuiltIn    ();
};


void CharInfoBuilder::Run()
{
	m_chars.Clear();
	m_chars.Resize(Unicode::MaxAssignedCodePoint + 1);

	Str unicodeDataFileName    { JoinPath(m_dir, "UnicodeData.txt"       ) };
	Str eastAsianWidthFileName { JoinPath(m_dir, "EastAsianWidth.txt"    ) };
	Str unifontFileName        { JoinPath(m_dir, "unifont.sfd"           ) };
	Str unifontUpperFileName   { JoinPath(m_dir, "unifont_upper.sfd"     ) };
	Str outputFileName         { JoinPath(m_dir, "AtUnicodeCharInfo.cpp" ) };

	ReadCharCategories  (unicodeDataFileName);
	SetDefaultWidths    ();
	ReadUnifontWidths   (unifontFileName);
	ReadUnifontWidths   (unifontUpperFileName);
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
		case Unicode::Category::Other_NotAssigned:
		case Unicode::Category::Other_PrivateUse:
		case Unicode::Category::Other_Surrogate:
		case Unicode::Category::Mark_SpacingCombining:
		case Unicode::Category::Mark_Enclosing:
		case Unicode::Category::Mark_Nonspacing:
		case Unicode::Category::Symbol_Modifier:
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


void CharInfoBuilder::ReadUnifontWidths(Seq fileName)
{
	Console::Out(Str("\r\n"
					 "Reading Unifont character widths from ").Add(fileName).Add("\r\n"));

	sizet nrProcessed {};
	sizet nrNarrow    {};
	sizet nrWide      {};
	sizet nrIgnored   {};
	sizet nrNoCat     {};
	sizet nrErrors    {};

	FileLoader fileLoader { fileName };
	LineReader lineReader { fileLoader };
	Str        lineStr;
	bool       inChar     {};
	uint       codePoint  { UINT_MAX };
	uint       width      { UINT_MAX };

	auto onError = [&] (Seq msg)
		{ ++nrErrors; Console::Out(Str("Error at line ").UInt(lineReader.LineNr()).Add(", code point ").Fun(FormatCodePoint, codePoint).Add(": ").Add(msg).Add("\r\n")); };

	while (lineReader.ReadLine(lineStr))
	{
		Seq line { lineStr };
		if (line.StartsWithExact("StartChar:"))
		{
			inChar = true;
			codePoint = UINT_MAX;
			width = UINT_MAX;
		}
		else if (inChar)
		{
			if (line.StripPrefixExact("Encoding: "))
				codePoint = line.ReadNrUInt32Dec();
			else if (line.StripPrefixExact("Width: "))
				width = line.ReadNrUInt32Dec();

			if (codePoint != UINT_MAX && width != UINT_MAX)
			{
				inChar = false;

				if (codePoint >= m_chars.Len())
					++nrIgnored;
				else
				{
					CharEntry& entry { m_chars[codePoint] };

					if (!entry.m_haveCategory)
						++nrNoCat;

					uint w { UINT_MAX };
					     if (width ==    0) w = 0;
					else if (width ==  512) w = 1;
					else if (width == 1024) w = 2;
					else
						onError(Str("Unexpected width ").UInt(width));

					if (w != UINT_MAX)
					{
						if (entry.m_haveUnifontWidth)
						{
							if (w != entry.m_ci.m_width)
								onError(Str("Duplicate code point with conflicting width: ").UInt(w).Add(" instead of ").UInt(entry.m_ci.m_width));
						}
						else
						{
							if (w == 1)
								++nrNarrow;
							else if (w == 2)
								++nrWide;

							++nrProcessed;
							entry.m_ci.m_width = w;
							entry.m_haveUnifontWidth = true;
						}
					}
				}
			}
		}
	}

	Console::Out(Str().UInt(nrProcessed).Add(" processed, ").UInt(nrNarrow).Add(" narrow, ").UInt(nrWide).Add(" wide, ")
				.UInt(nrErrors).Add(" errors, ").UInt(nrNoCat).Add(" without category, ").UInt(nrIgnored).Add(" ignored\r\n"));
}


void CharInfoBuilder::ReadEastAsianWidths(Seq fileName)
{
	Console::Out(Str("\r\n"
					 "Reading East Asian widths from ").Add(fileName).Add("\r\n"));

	sizet nrProcessed    {};
	sizet nrNarrow       {};
	sizet nrWide         {};
	sizet nrErrors       {};
	sizet nrNoCat        {};
	sizet nrIgnored      {};
	sizet nrNoUnifontW   {};
	sizet nrDiffUnifontW {};

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

						if (!entry.m_haveCategory)     ++nrNoCat;
						if (!entry.m_haveUnifontWidth) ++nrNoUnifontW;

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

							if (entry.m_haveUnifontWidth && w != entry.m_ci.m_width)
							{
								++nrDiffUnifontW;
								Console::Out(Str("Line ").UInt(csvReader.LineNr()).Add(", code point ").Fun(FormatCodePoint, codePoint)
											.Add(": Overriding Unifont width: ").UInt(w).Add(" instead of ").UInt(entry.m_ci.m_width).Add("\r\n"));
							}

							++nrProcessed;
							entry.m_ci.m_width = w;
							entry.m_haveEastAsianWidth = true;
						}
					}
				}
			}
		}
	}

	uint nrKeepUnifontW {};
	for (CharEntry const& entry : m_chars)
		if (entry.m_haveUnifontWidth && !entry.m_haveEastAsianWidth)
			++nrKeepUnifontW;

	Console::Out(Str().UInt(nrProcessed).Add(" processed, ").UInt(nrNarrow).Add(" narrow, ").UInt(nrWide).Add(" wide, ").UInt(nrErrors).Add(" errors, ")
				.UInt(nrNoCat).Add(" without category, ").UInt(nrIgnored).Add(" ignored\r\n").UInt(nrNoUnifontW).Add(" did not have Unifont width, ")
				.UInt(nrDiffUnifontW).Add(" with Unifont width overridden, ").UInt(nrKeepUnifontW).Add(" keep Unifont width\r\n"));
}


void CharInfoBuilder::Sanitize()
{
	// We perform similar sanitizations as charwidths.jl from utf8proc:
	// 
	// https://github.com/JuliaLang/utf8proc/blob/master/data/charwidths.jl
	//
	// However, we do not set zero widths for categories Other_NotAssigned (Cn) and Other_PrivateUse (Co).

	sizet nrSanitized {};

	for (uint codePoint=0; codePoint!=m_chars.Len(); ++codePoint)
	{
		CharEntry& entry { m_chars[codePoint] };
		bool setZeroWidth {};

		if (entry.m_ci.m_cat == Unicode::Category::Mark_Nonspacing ||
		    entry.m_ci.m_cat == Unicode::Category::Other_Control)
		{
			// Unifont has width 2 glyphs for ASCII control characters
			setZeroWidth = true;
		}
		else if (entry.m_ci.m_cat == Unicode::Category::Other_Format)
		{
			// Characters in category Cf (Other_Format) have width zero, except these Arabic characters
			if (codePoint != 0x0601 && codePoint != 0x0602 && codePoint != 0x0603 && codePoint != 0x06DD)
				setZeroWidth = true;
		}

		if (setZeroWidth && entry.m_ci.m_width != 0)
		{
			++nrSanitized;
			entry.m_ci.m_width = 0;
		}
	}

	Console::Out(Str("\r\n")
				.UInt(nrSanitized).Add(" sanitized (non-zero width set to zero)\r\n"));
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
	}

	Console::Out(Str().UInt(nrChangedBoth).Add(" change category and width, ").UInt(nrChangedCat).Add(" change category only, ")
				.UInt(nrChangedWidth).Add(" change width only, ").UInt(nrSame).Add(" stay the same\r\n"));
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
					 "\r\n"
					 "Requires the following files in the specified data directory:\r\n"
					 "\r\n"
					 "\r\n"
					 "- UnicodeData.txt and EastAsianWidth.txt from the Unicode Character Database:\r\n"
					 "\r\n"
					 "  http://www.unicode.org/Public/UCD/latest/ucd/ \r\n"
					 "\r\n"
					 "\r\n"
					 "- unifont.sfd and unifont_upper.sfd generated from the latest Unifont TTFs at:\r\n"
					 "\r\n"
					 "  http://unifoundry.com/unifont.html \r\n"
					 "\r\n"
					 "  To convert TTF to SFD, install FontForge as part of Cygwin, and run in bash:\r\n"
					 "\r\n"
					 "  fontforge -lang=ff -c \"Open(\\\"<file>.ttf\\\");Save(\\\"<file>.sfd\\\");Quit(0);\"\r\n"
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
