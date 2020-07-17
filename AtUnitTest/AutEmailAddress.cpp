#include "AutIncludes.h"
#include "AutMain.h"


void TestEmailAddress(Seq addr)
{
	Console::Out(Str("\r\nAddress:    <").Add(addr).Add(">\r\n"));
		
	ParseTree ptAddr { addr };
	ptAddr.RecordBestToStack();
	if (!ptAddr.Parse(Imf::C_addr_spec))
	{
		ParseTree ptList { addr };
		ptList.RecordBestToStack();
		if (!ptList.Parse(Imf::C_casual_addr_list))
			Console::Out(Str::From(ptAddr, ParseTree::BestAttempt).Add("\r\n"));
		else
		{
			Str dump;
			ptList.Root().Dump(dump);
			Console::Out(dump.Add("\r\n"));
		}
	}
	else
	{
		Str localPart, domain;
		Imf::ExtractLocalPartAndDomainFromEmailAddress(addr, &localPart, &domain);
		Console::Out(Str("Local part: <").Add(localPart).Add(">\r\n"));
		Console::Out(Str("Domain:     <").Add(domain).Add(">\r\n"));

		if (Imf::IsValidDomain(domain))
			Console::Out("Domain is VALID according to RFC 5322\r\n");
		else
			Console::Out("Domain is NOT valid according to RFC 5322\r\n");

		Str dump;
		ptAddr.Root().Dump(dump);
		Console::Out(dump.Add("\r\n"));
	}

	Console::Out("\r\nEnumerating addresses:\r\n");

	Imf::ForEachAddressResult result = Imf::ForEachAddressInCasualEmailAddressList(addr, [&] (Str&& addr) -> bool
		{
			Console::Out(Str(addr).Add("\r\n"));
			return true;
		} );

	if (result.m_parseError.Any())
		Console::Out(Str::Join("Address(es) could not be parsed: ", result.m_parseError, "\r\n"));
	else
		Console::Out(Str().UInt(result.m_nrEnumerated).Add(" address(es) enumerated\r\n"));
}


void EmailAddressTest(Slice<Seq> args)
{
	try
	{
		if (args.Len() < 3)
		{
			TestEmailAddress("(!) \"foo;bar,shar!\" (woof \r\n \"\"\") @ (blah; zar, far) +-!~.example.com (???)");
			TestEmailAddress("aa@bb; group:aa@bb,cc@dd;, xx@yy, \";,_\" (uff) @ (f;u,f) example.com, "
				"multiline\r\n @example.com; multiline(\r\n )@example.com; multiline@(\r\n )example.com");
		}
		else
		{
			for (sizet i=2; i!=args.Len(); ++i)
				TestEmailAddress(args[i]);
		}
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Exception:\r\n").Add(e.what()).Add("\r\n"));
	}
}
