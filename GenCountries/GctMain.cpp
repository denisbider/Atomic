#include "GctIncludes.h"
#include "GctMain.h"


int DisplayHelp()
{
	Console::Out(
		"This program, GenCountries, is Copyright (C) 2017-2019 by denis bider.\r\n"
		"The author permits use of this program free of charge, for any lawful purpose,\r\n"
		"except direct modification of the executable. No warranty is given or implied.\r\n"
		"This author does not impose restrictions on your use of this program's output,\r\n"
		"but your rights on the output ARE limited by your rights on input you provide.\r\n"
		"\r\n"
		"Usage:\r\n"
		"  GenCountries [-in <inDir>] [-out <outDir>]\r\n"
		"\r\n"
		"<inDir> must contain files:\r\n"
		"  GeoLite2-Country-Blocks-IPv4.csv\r\n"
		"  GeoLite2-Country-Blocks-IPv6.csv\r\n"
		"  GeoLite2-Country-Locations-en.csv\r\n"
		"\r\n"
		"<outDir> receives the file:\r\n"
		"  Countries.bin\r\n"
		"\r\n"
		"If any output files already exist, they are overwritten.\r\n"
		"\r\n"
		"At least -in or -out must be specified:\r\n"
		"- Only -in: input is read and analyzed, but output not written.\r\n"
		"- Only -out: any previously generated output is validated.\r\n"
		"- Both: input is read and analyzed; output is written and validated.\r\n");
	return 2;
}


int main(int argc, char** argv)
{
	struct WaitOnExit {
		~WaitOnExit() { if (IsDebuggerPresent()) { Console::Out("\r\nPress any key\r\n"); Console::ReadChar(); } }
	} waitOnExit;

	Console::Out("\r\n");

	if (argc == 1)
		return DisplayHelp();

	auto readDirArg = [argc, argv] (int& i, Str& dir) -> bool
		{
			if (++i == argc) { Console::Out("Missing directory name\r\n"); return false; }
			dir = argv[i];
			return true;
		};

	Opts opts;
	for (int i=1; i!=argc; ++i)
	{
		Seq arg = argv[i];
		     if (arg.EqualInsensitive("-in"  )) { if (!readDirArg(i, opts.m_inDir .Init() )) return 2; }
		else if (arg.EqualInsensitive("-out" )) { if (!readDirArg(i, opts.m_outDir.Init() )) return 2; }
		else if (arg.EqualExact("-?") || arg.EqualExact("/?") || arg.EqualInsensitive("-h") || arg.EqualInsensitive("/h"))
			return DisplayHelp();
		else
		{
			Console::Out(Str("Unrecognized parameter: ").Add(arg).Add("\r\n"));
			return 2;
		}
	}

	if (!opts.m_inDir.Any() && !opts.m_outDir.Any())
	{
		Console::Out("Missing -in and/or -out\r\n");
		return 2;
	}

	try
	{
		GctData data { opts };
		if (opts.m_inDir.Any())
		{
			data.Read();
			data.Analyze();
			data.Write();
		}
		if (opts.m_outDir.Any())
			data.Validate();
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Terminated by std::exception:\r\n").Add(e.what()).Add("\r\n"));
		return 1;
	}

	return 0;
}
