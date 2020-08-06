#include "AutIncludes.h"
#include "AutMain.h"


void ShowCharInfo(uint c)
{
	Unicode::CharInfo ci = Unicode::CharInfo::Get(c);
	Str line;
	line.UInt(c).Add(" U+").UInt(c, 16, 4).Ch(' ')
		.Add(Unicode::Category::ValueToName(ci.m_cat)).Add(" (")
		.Add(Unicode::Category::ValueToCode(ci.m_cat)).Add(") width ")
		.UInt(ci.m_width).Add("\r\n");
	Console::Out(line);
}


void CharInfoTest(Args& args)
{
	if (!args.Any())
		ShowCharInfo(0x27F3);
	else
		do
		{
			Seq arg = args.Next();

			uint c;
			if (arg.StripPrefixInsensitive("0x"))
				c = arg.ReadNrUInt32(16);
			else
				c = arg.ReadNrUInt32Dec();

			if (c == UINT32_MAX || arg.n)
				Console::Out(Str::Join("Expecting decimal or hexadecimal code point: ", arg, "\r\n"));
			else
				ShowCharInfo(c);
		}
		while (args.Any());
}
