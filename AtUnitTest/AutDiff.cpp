#include "AutIncludes.h"
#include "AutMain.h"


Str DescInputUnit(Seq src, Diff::InputUnit const& iu)
{
	Str s;
	s.ReserveExact(src.n + 1 + 15 + 1 + iu.m_value.n + 2);
	return s.Add(src).Ch(':').UInt(iu.m_seqNr).Ch(':').Add(iu.m_value).Add("\r\n");
}


void CheckDiffCorrect(PtrPair<Diff::DiffUnit> diff, PtrPair<Diff::InputUnit> inputOld, PtrPair<Diff::InputUnit> inputNew)
{
	auto expectLine = [] (Seq src, PtrPair<Diff::InputUnit>& input, Diff::InputUnit const& iu, sizet& linesMatched) -> bool
		{
			if (!input.Any())
			{
				Str desc = DescInputUnit("exp", iu);
				Console::Err(Str::Join(src, " input is empty, expecting:\r\n", desc));
				return false;
			}
			
			if (input.First().m_value != iu.m_value)
			{
				Str srcDesc = DescInputUnit(src, input.First());
				Str expDesc = DescInputUnit("exp", iu);
				Console::Err(Str::Join(src, " input does not match expected:\r\n", srcDesc, expDesc));
				return false;
			}
			
			if (input.First().m_seqNr != iu.m_seqNr)
			{
				Str srcDesc = DescInputUnit(src, input.First());
				Str expDesc = DescInputUnit("exp", iu);
				Console::Err(Str::Join(src, " sequence number does not match expected:\r\n", srcDesc, expDesc));
				return false;
			}

			input.PopFirst();
			++linesMatched;
			return true;
		};

	sizet diffLines {}, sameLines {}, addLines {}, removeLines {}, oldLines {}, newLines {};

	auto expectSameLine = [&] () -> bool
		{
			Seq valueOld = inputOld.Any() ? inputOld.First().m_value : Seq();
			Seq valueNew = inputNew.Any() ? inputNew.First().m_value : Seq();
			Diff::InputUnit iuOld { oldLines+1, valueNew };
			Diff::InputUnit iuNew { newLines+1, valueOld };
			if (!expectLine("old", inputOld, iuOld, oldLines)) return false;
			if (!expectLine("new", inputNew, iuNew, newLines)) return false;
			++sameLines;
			return true;
		};

	bool ok {};
	for (Diff::DiffUnit const& u : diff)
	{
		switch (u.m_disposition)
		{
		case Diff::DiffDisposition::Added:
			while (u.m_inputUnit.m_seqNr > newLines+1)
				if (!expectSameLine()) goto Fail;
			if (!expectLine("new", inputNew, u.m_inputUnit, newLines)) goto Fail;
			++addLines;
			break;

		case Diff::DiffDisposition::Removed:
			while (u.m_inputUnit.m_seqNr > oldLines+1)
				if (!expectSameLine()) goto Fail;
			if (!expectLine("old", inputOld, u.m_inputUnit, oldLines)) goto Fail;
			++removeLines;
			break;

		default: EnsureThrow(!"Unexpected DiffDisposition");
		}

		++diffLines;
	}

	if (diffLines == diff.Len())
	{
		while (inputOld.Len())
			if (!expectSameLine()) goto Fail;

		ok = true;
	}

Fail:
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

	if (!ok)
		throw "Diff incorrect";

	Console::Err(Str("OK, matched ").UInt(diffLines).Add(" diff lines (")
		.UInt(sameLines).Add(" same, ").UInt(addLines).Add(" added, ").UInt(removeLines).Add(" removed), ")
		.UInt(oldLines).Add(" old lines, ").UInt(newLines).Add(" new lines\r\n"));
}


void DiffTest(Seq linesOld, Seq linesNew, Seq desc, Diff::DiffParams const& diffParams)
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
	Diff::Generate(inputOld, inputNew, diff, diffParams);

	if (!desc.n)
	{
		Time elapsed = Time::NonStrictNow() - timeBefore;
		Console::Err(Str("Elapsed: ").Obj(elapsed, TimeFmt::DurationMilliseconds).Add("\r\n"));
	}

	Str outLine;
	for (Diff::DiffUnit const& u : diff)
	{
		outLine.ReserveAtLeast(20 + u.m_inputUnit.m_value.n + 2);

		switch (u.m_disposition)
		{
		case Diff::DiffDisposition::Added:     outLine.Set("+:").UInt(u.m_inputUnit.m_seqNr).Ch(':'); break;
		case Diff::DiffDisposition::Removed:   outLine.Set("-:").UInt(u.m_inputUnit.m_seqNr).Ch(':'); break;
		default: EnsureThrow(!"Unexpected DiffDisposition");
		}

		outLine.Add(u.m_inputUnit.m_value).Add("\r\n");
		Console::Out(outLine);
	}

	if (desc.n)
		Console::Out("---end---\r\n");

	CheckDiffCorrect(diff, inputOld, inputNew);
}


void DiffTestSimple(Seq oldText, Seq newText, Diff::DiffParams const& diffParams)
{
	Str oldLines, newLines;
	oldLines.ReserveExact(2*oldText.n);
	newLines.ReserveExact(2*newText.n);

	for (byte c : oldText) oldLines.Byte(c).Byte(10);
	for (byte c : newText) newLines.Byte(c).Byte(10);

	if (!oldText.n) oldText = "_";
	if (!newText.n) newText = "_";
	Str desc = Str::Join(oldText, " => ", newText);

	DiffTest(oldLines, newLines, desc, diffParams);
}


void DiffTests(int argc, char** argv)
{
	Diff::DiffParams diffParams;

	bool helpRequested {}, simpleInput {}, haveOldParam {}, haveNewParam {};
	Seq oldParam, newParam;

	for (int i=2; i<argc; ++i)
	{
		Seq arg = argv[i];
		if (arg.StartsWithExact("-"))
		{
			if (arg == "-?")
			{
				helpRequested = true;
				break;
			}
			else if (arg == "-s")
				simpleInput = true;
			else if (arg == "-nls")
				diffParams.m_limitSteps = false;
			else
				{ Console::Err(Str::Join("Unrecognized switch: ", arg, "\r\n")); return; }
		}
		else if (!haveOldParam)
		{
			oldParam = arg;
			haveOldParam = true;
		}
		else if (!haveNewParam)
		{
			newParam = arg;
			haveNewParam = true;
		}
		else
			{ Console::Err(Str::Join("Unexpected parameter: ", arg, "\r\n")); return; }
	}

	if (helpRequested)
	{
		Console::Out(
			"Usage: AtUnitTest diff [<oldFile> <newFile> | -s <oldText> <newText>] [options]\r\n"
			"Options:\r\n"
			" -nls    Set DiffParams::m_limitSteps to false\r\n");
	}
	else
	{
		if (!haveOldParam && !haveNewParam)
		{
			DiffTestSimple("",            "",        diffParams);
			DiffTestSimple("",            "a",       diffParams);
			DiffTestSimple("a",           "",        diffParams);
			DiffTestSimple("a",           "a",       diffParams);
			DiffTestSimple("a",           "b",       diffParams);
			DiffTestSimple("a",           "ab",      diffParams);
			DiffTestSimple("a",           "ba",      diffParams);
			DiffTestSimple("ab",          "a",       diffParams);
			DiffTestSimple("ba",          "a",       diffParams);
			DiffTestSimple("ab",          "ab",      diffParams);
			DiffTestSimple("ab",          "bca",     diffParams);
			DiffTestSimple("bca",         "ab",      diffParams);
			DiffTestSimple("abcd",        "bcda",    diffParams);
			DiffTestSimple("bcda",        "abcd",    diffParams);
			DiffTestSimple("abcoeppklmn", "op",      diffParams);
			DiffTestSimple("abcabba",     "cbabac",  diffParams);
			DiffTestSimple("abgdef",      "gh",      diffParams);
			DiffTestSimple("xaxcxabc",    "abcy",    diffParams);
		}
		else if (!haveNewParam)
		{
			Console::Err("Expecting <newText> or <newFile>\r\n");
		}
		else if (simpleInput)
			DiffTestSimple(oldParam, newParam, diffParams);
		else
		{
			Str linesOld, linesNew;
			File().Open(oldParam, File::OpenArgs::DefaultRead()).ReadAllInto(linesOld);
			File().Open(newParam, File::OpenArgs::DefaultRead()).ReadAllInto(linesNew);

			DiffTest(linesOld, linesNew, Seq(), diffParams);
		}
	}
}
