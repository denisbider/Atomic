#include "AutIncludes.h"
#include "AutMain.h"


Str DescInputUnit(Seq src, Diff::InputUnit const& iu)
{
	Str s;
	s.ReserveExact(src.n + 2 + 15 + 2 + iu.m_value.n + 2);
	return s.Add(src).Add(": ").UInt(iu.m_seqNr).Add(": ").Add(iu.m_value).Add("\r\n");
}


void CheckDiffCorrect(PtrPair<Diff::DiffUnit> diff, PtrPair<Diff::InputUnit> inputOld, PtrPair<Diff::InputUnit> inputNew)
{
	auto expectLine = [] (Seq src, PtrPair<Diff::InputUnit>& input, Diff::InputUnit const& iu, bool checkSeqNr, sizet& linesMatched) -> bool
		{
			if (!input.Any())
			{
				Str desc = DescInputUnit("diff", iu);
				Console::Err(Str::Join(src, " input is empty, expecting:\r\n", desc));
				return false;
			}
			
			if (input.First().m_value != iu.m_value)
			{
				Str srcDesc = DescInputUnit(src, input.First());
				Str diffDesc = DescInputUnit("diff", iu);
				Console::Err(Str::Join(src, " input does not match diff:\r\n", srcDesc, diffDesc));
				return false;
			}
			
			if (checkSeqNr && input.First().m_seqNr != iu.m_seqNr)
			{
				Str srcDesc = DescInputUnit(src, input.First());
				Str diffDesc = DescInputUnit("diff", iu);
				Console::Err(Str::Join(src, " sequence number does not match diff:\r\n", srcDesc, diffDesc));
				return false;
			}

			input.PopFirst();
			++linesMatched;
			return true;
		};

	sizet diffLines {}, oldLines {}, newLines {};
	bool ok = true;
	for (Diff::DiffUnit const& u : diff)
	{
		switch (u.m_disposition)
		{
		case Diff::DiffDisposition::Unchanged:
			ok = ok && expectLine("old", inputOld, u.m_inputUnit, false, oldLines);
			ok = ok && expectLine("new", inputNew, u.m_inputUnit, true,  newLines);
			break;

		case Diff::DiffDisposition::Added:
			ok = ok && expectLine("new", inputNew, u.m_inputUnit, true, newLines);
			break;

		case Diff::DiffDisposition::Removed:
			ok = ok && expectLine("old", inputOld, u.m_inputUnit, true, oldLines);
			break;

		default: EnsureThrow(!"Unexpected DiffDisposition");
		}
		
		if (!ok)
			break;

		++diffLines;
	}

	if (ok && inputOld.Any())
	{
		Str desc = DescInputUnit("old", inputOld.First());
		Console::Err(Str::Join("Unexpected old input remaining after end of diff:\r\n", desc));
		ok = false;
	}

	if (ok && inputNew.Any())
	{
		Str desc = DescInputUnit("new", inputNew.First());
		Console::Err(Str::Join("Unexpected new input remaining after end of diff:\r\n", desc));
		ok = false;
	}

	if (ok)
		Console::Err(Str("OK, matched ").UInt(diffLines).Add(" diff lines, ").UInt(oldLines).Add(" old lines, ").UInt(newLines).Add(" new lines\r\n"));
}


void DiffTest(Seq linesOld, Seq linesNew, Seq desc)
{
	if (desc.n)
		Console::Out(Str::Join("\r\n", desc, "\r\n---begin---\r\n"));

	Vec<Diff::InputUnit> inputOld, inputNew;

	sizet seqNr {};
	linesOld.ForEachLine( [&] (Seq line) -> bool { inputOld.Add(++seqNr, line); return true; } );

	seqNr = 0;
	linesNew.ForEachLine( [&] (Seq line) -> bool { inputNew.Add(++seqNr, line); return true; } );

	Time timeBefore;
	if (!desc.n)
		timeBefore = Time::NonStrictNow();

	Vec<Diff::DiffUnit> diff;
	Diff::Generate(inputOld, inputNew, diff, Diff::DiffParams());

	if (!desc.n)
	{
		Time elapsed = Time::NonStrictNow() - timeBefore;
		Console::Err(Str("Elapsed: ").Obj(elapsed, TimeFmt::DurationMilliseconds).Add("\r\n"));
	}

	Str outLine;
	for (Diff::DiffUnit const& u : diff)
	{
		outLine.ReserveAtLeast(2 + u.m_inputUnit.m_value.n + 2);

		switch (u.m_disposition)
		{
		case Diff::DiffDisposition::Unchanged: outLine.Set("  "); break;
		case Diff::DiffDisposition::Added:     outLine.Set("+ "); break;
		case Diff::DiffDisposition::Removed:   outLine.Set("- "); break;
		default: EnsureThrow(!"Unexpected DiffDisposition");
		}

		outLine.Add(u.m_inputUnit.m_value).Add("\r\n");
		Console::Out(outLine);
	}

	if (desc.n)
		Console::Out("---end---\r\n");

	CheckDiffCorrect(diff, inputOld, inputNew);
}


void DiffTestSimple(Seq oldText, Seq newText)
{
	Str oldLines, newLines;
	oldLines.ReserveExact(2*oldText.n);
	newLines.ReserveExact(2*newText.n);

	for (byte c : oldText) oldLines.Byte(c).Byte(10);
	for (byte c : newText) newLines.Byte(c).Byte(10);

	if (!oldText.n) oldText = "_";
	if (!newText.n) newText = "_";
	Str desc = Str::Join(oldText, " => ", newText);

	DiffTest(oldLines, newLines, desc);
}


void DiffTests(int argc, char** argv)
{
	if (argc <= 2)
	{
		DiffTestSimple("",            ""         );
		DiffTestSimple("",            "a"        );
		DiffTestSimple("a",           ""         );
		DiffTestSimple("a",           "a"        );
		DiffTestSimple("a",           "b"        );
		DiffTestSimple("a",           "ab"       );
		DiffTestSimple("a",           "ba"       );
		DiffTestSimple("ab",          "a"        );
		DiffTestSimple("ba",          "a"        );
		DiffTestSimple("ab",          "ab"       );
		DiffTestSimple("ab",          "bca"      );
		DiffTestSimple("bca",         "ab"       );
		DiffTestSimple("abcd",        "bcda"     );
		DiffTestSimple("bcda",        "abcd"     );
		DiffTestSimple("abcoeppklmn", "op"       );
		DiffTestSimple("abcabba",     "cbabac"   );
		DiffTestSimple("abgdef",      "gh"       );
		DiffTestSimple("xaxcxabc",    "abcy"     );
	}
	else if (argc == 5)
	{
		if (!Seq(argv[2]).EqualInsensitive("-s"))
			Console::Out("Expecting: -s <oldText> <newText>\r\n");
		else
			DiffTestSimple(argv[3], argv[4]);
	}
	else if (argc != 4)
	{
		Console::Out("Expecting parameters: <oldFile> <newFile>\r\n");
	}
	else
	{
		Seq oldFile = argv[2];
		Seq newFile = argv[3];

		Str linesOld, linesNew;
		File().Open(oldFile, File::OpenArgs::DefaultRead()).ReadAllInto(linesOld);
		File().Open(newFile, File::OpenArgs::DefaultRead()).ReadAllInto(linesNew);

		DiffTest(linesOld, linesNew, Seq());
	}
}
