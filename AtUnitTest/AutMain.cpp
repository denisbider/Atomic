#include "AutIncludes.h"
#include "AutMain.h"


int main(int argc, char** argv)
{
	int exitCode {};

	try
	{
		if (argc < 2)
		{
			Console::Out(
				"AtUnitTest commands:\r\n"
				"  core\r\n"
				"  diff - Diff\r\n"
				"  mkdn - Markdown\r\n"
				"  text - TextBuilder\r\n"
				"  mltp - Multipart\r\n"
				"  schc - SchannelClient\r\n"
				"  smtr - SmtpReceiver\r\n"
				"  werr - WinErr\r\n"
				"  addr - EmailAddress\r\n"
				"  dkim - Dkim\r\n"
				"  boot - BootTime\r\n"
				"  ents - EntityStore\r\n"
				"  lrge - LargeEntities\r\n"
				"  bcrp - BCrypt\r\n"
				"  rsas - RsaSigner\r\n"
				"  mpui - MpUInt\r\n"
				"  base - BaseXY\r\n"
				"  actv - Actv\r\n");
		}
		else if (0 == _stricmp(argv[1], "core"))	// Can't use Seq if that's what we're testing
		{
			CoreTests();
		}
		else
		{
			Seq cmd { argv[1] };
				 if (cmd.EqualInsensitive("diff")) { DiffTests          (argc, argv); }
			else if (cmd.EqualInsensitive("mkdn")) { MarkdownTests      (argc, argv); }
			else if (cmd.EqualInsensitive("text")) { TextBuilderTests   ();           }
			else if (cmd.EqualInsensitive("mltp")) { MultipartTests     ();           }
			else if (cmd.EqualInsensitive("schc")) { SchannelClientTest (argc, argv); }
			else if (cmd.EqualInsensitive("smtr")) { SmtpReceiverTest   ();           }
			else if (cmd.EqualInsensitive("werr")) { WinErrTest         (argc, argv); }
			else if (cmd.EqualInsensitive("addr")) { EmailAddressTest   (argc, argv); }
			else if (cmd.EqualInsensitive("dkim")) { DkimTest           (argc, argv); }
			else if (cmd.EqualInsensitive("boot")) { BootTime           ();           }
			else if (cmd.EqualInsensitive("ents")) { EntityStoreTests   (argc, argv); }
			else if (cmd.EqualInsensitive("lrge")) { LargeEntitiesTests (argc, argv); }
			else if (cmd.EqualInsensitive("bcrp")) { BCryptTests        (argc, argv); }
			else if (cmd.EqualInsensitive("rsas")) { RsaSignerTests     (argc, argv); }
			else if (cmd.EqualInsensitive("mpui")) { MpUIntTests        (argc, argv); }
			else if (cmd.EqualInsensitive("base")) { BaseXYTests        (argc, argv); }
			else if (cmd.EqualInsensitive("actv")) { ActvTests          (argc, argv); }
			else
			{
				Console::Out("Unrecognized command\r\n");
				exitCode = 2;
			}
		}
	}
	catch (char const* z)
	{
		Console::Out(Str(z).Add("\r\n"));
		exitCode = 1;
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Terminated by unexpected exception:\r\n").Add(e.what()).Add("\r\n"));
		exitCode = 1;
	}

	if (IsDebuggerPresent())
	{
		Console::Out(Str("\r\nProgram exiting with exit code ").SInt(exitCode).Add(". Press any key to exit\r\n"));
		Console::ReadChar();
	}

	return exitCode;
}
