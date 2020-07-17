#include "AutIncludes.h"
#include "AutMain.h"


int wmain(int argc, wchar_t const* const* argv)
{
	int exitCode {};

	try
	{
		if (argc < 2)
		{
			Console::Out(
				"AtUnitTest commands:\r\n"
				"  core\r\n"
				"  actv - Actv\r\n"
				"  base - BaseXY\r\n"
				"  bcrp - BCrypt\r\n"
				"  boot - BootTime\r\n"
				"  diff - Diff\r\n"
				"  dkim - Dkim\r\n"
				"  addr - EmailAddress\r\n"
				"  ents - EntityStore\r\n"
				"  htme - HtmlEmbed\r\n"
				"  lrge - LargeEntities\r\n"
				"  mapc - Map\r\n"
				"  mkdn - Markdown\r\n"
				"  mpui - MpUInt\r\n"
				"  mltp - Multipart\r\n"
				"  rsas - RsaSigner\r\n"
				"  schc - SchannelClient\r\n"
				"  smtr - SmtpReceiver\r\n"
				"  uris - Uri\r\n"
				"  text - TextBuilder\r\n"
				"  werr - WinErr\r\n");
		}
		else if (0 == _wcsicmp(argv[1], L"core"))	// Can't use Seq if that's what we're testing
		{
			CoreTests();
		}
		else
		{
			Args args { argc, argv };
			args.NextOrErr("Command line does not contain executable path");
			Seq cmd = args.NextOrErr("Missing command");

			if (false) {}
			else if (cmd.EqualInsensitive("actv")) { ActvTests          ();                              }
			else if (cmd.EqualInsensitive("base")) { BaseXYTests        ();                              }
			else if (cmd.EqualInsensitive("bcrp")) { BCryptTests        ();                              }
			else if (cmd.EqualInsensitive("boot")) { BootTime           ();                              }
			else if (cmd.EqualInsensitive("diff")) { DiffTests          (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("dkim")) { DkimTest           (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("addr")) { EmailAddressTest   (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("ents")) { EntityStoreTests   (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("htme")) { HtmlEmbedTest      (args);                          }
			else if (cmd.EqualInsensitive("lrge")) { LargeEntitiesTests ();                              }
			else if (cmd.EqualInsensitive("mapc")) { MapTests           ();                              }
			else if (cmd.EqualInsensitive("mkdn")) { MarkdownTests      (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("mpui")) { MpUIntTests        ();                              }
			else if (cmd.EqualInsensitive("mltp")) { MultipartTests     ();                              }
			else if (cmd.EqualInsensitive("rsas")) { RsaSignerTests     ();                              }
			else if (cmd.EqualInsensitive("text")) { TextBuilderTests   ();                              }
			else if (cmd.EqualInsensitive("schc")) { SchannelClientTest (args.ConvertAll().Converted()); }
			else if (cmd.EqualInsensitive("smtr")) { SmtpReceiverTest   ();                              }
			else if (cmd.EqualInsensitive("uris")) { UriTests           ();                              }
			else if (cmd.EqualInsensitive("werr")) { WinErrTest         (args.ConvertAll().Converted()); }
			else throw Args::Err("Unrecognized command");
		}
	}
	catch (Args::Err const& e)
	{
		Console::Out(Str(e.S()).Add("\r\n"));
		exitCode = 2;
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
