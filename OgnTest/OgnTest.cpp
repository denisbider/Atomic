// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


int main(int argc, char const* const* argv)
{
	int exitCode = -1;

	try
	{
		CmdlArgs args { argc, argv };
		args.Skip();

		if (!args.More())
		{
			std::cout << "Originator test utility" << std::endl
				<< "Copyright (C) 2018-2019 by denis bider. All rights reserved." << std::endl
				<< std::endl
				<< "Usage:" << std::endl
				<< std::endl
				<< "enums - display Originator enumeration values, names and descriptions" << std::endl
				<< std::endl
				<< "addrs [file] - read file as single multiline string, parse as list of addresses" << std::endl
				<< "  If no file provided, output some hardcoded addresses that can serve as input" << std::endl
				<< std::endl
				<< "dkimgen <outFile> - generate a DKIM keypair" << std::endl
				<< std::endl
				<< "dkimpub <file> - read DKIM private key from file, output public key" << std::endl
				<< std::endl
				<< "genmsg [-o <outFile>] [-mkdn <inFile>]" << std::endl
				<< "  [-from <addr>] [-to <addr>]* [-cc <addr>]* [-sub <subject>]" << std::endl
				<< "  [-kp <file> -sdid <sdid> -sel <selector>] [-attach <type> <file>]*" << std::endl
				<< std::endl
				<< "run [-stgs <stgsFile>]" << std::endl
				<< std::endl
				<< "sendmsg [-stgs <stgsFile>] [-retry <minsList>] [-tlsreq <tlsAssurance>]" << std::endl
				<< "  [-addldomain <domain>]* [-basesecsmax <nr>] [-minbps <nr>] [-from <addr>]" << std::endl
				<< "  [-mbox <mailbox>]* [-todomain <domain>] [-content <msgFile>] [-ctx <context>]" << std::endl;
			exitCode = 2;
		}
		else
		{
			std::string cmd = args.Next();
			if (cmd == "enums")
			{
				if (args.More())
					throw UsageErr("enums: Unexpected parameters");

				CmdEnums();
			}
			else if (cmd == "addrs")
			{
				std::string inFile;
				if (args.More())
				{
					inFile = args.Next();
					if (args.More())
						throw UsageErr("addrs: Unexpected number of parameters");
				}

				CmdAddrs(inFile);
			}
			else if (cmd == "dkimgen")
			{
				std::string outFile = args.Next();
				if (args.More())
					throw UsageErr("dkimgen: Unexpected number of parameters");

				CmdDkimGen(outFile);
			}
			else if (cmd == "dkimpub")
			{
				std::string kpFile = args.Next();
				if (args.More())
					throw UsageErr("dkimpub: Unexpected number of parameters");

				CmdDkimPub(kpFile);
			}
			else if (cmd == "genmsg")
			{
				CmdGenMsg(args);
			}
			else if (cmd == "run")
			{
				CmdRun(args);
			}
			else if (cmd == "sendmsg")
			{
				CmdSendMsg(args);
			}
			else
				throw UsageErr("Unrecognized command");

			exitCode = 0;
		}
	}
	catch (UsageErr const& e)
	{
		std::cout << e.what() << std::endl;
		exitCode = 2;
	}
	catch (std::exception const& e)
	{
		std::cout << "OgnTest main: Terminated by exception:" << std::endl
			<< e.what() << std::endl;
		exitCode = 1;
	}

	if (IsDebuggerPresent())
	{
		std::cout << "Press any key to exit" << std::endl;
		_getch();
	}

	return exitCode;
}
