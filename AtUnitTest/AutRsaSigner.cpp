#include "AutIncludes.h"
#include "AutMain.h"


void RsaSignerTests(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	try
	{
		Crypt::Initializer cryptInitializer;

		Console::Out("Generating keypair\r\n");
		
		RsaSigner signerA;
		signerA.Generate(2048);

		Console::Out("Keypair generated\r\n");

		Str privBin, pubA_bin, pubA_pkcs1, pubA_pkcs8;
		signerA.ExportPriv(privBin);
		signerA.ExportPub(pubA_bin);
		signerA.ExportPubPkcs1(pubA_pkcs1);
		signerA.ExportPubPkcs8(pubA_pkcs8);

		Console::Out("Importing private key\r\n");

		RsaSigner signerB;
		signerB.ImportPriv(privBin);

		Str pubB_bin, pubB_pkcs1, pubB_pkcs8;
		signerB.ExportPub(pubB_bin);
		signerB.ExportPubPkcs1(pubB_pkcs1);
		signerB.ExportPubPkcs8(pubB_pkcs8);

		if (pubA_bin == pubB_bin)
			Console::Out("Public keys are same, OK\r\n");
		else
			Console::Out(Str("Public keys are DIFFERENT:\r\n"
				"pubA:\r\n").HexDump(pubA_bin).Add("\r\n"
				"pubB:\r\n").HexDump(pubB_bin).Add("\r\n"));

		if (pubA_pkcs1 == pubB_pkcs1)
			Console::Out("PKCS#1 public keys are same, OK\r\n");
		else
			Console::Out(Str("PKCS#1 public keys are DIFFERENT:\r\n"
				"pubA:\r\n").HexDump(pubA_pkcs1).Add("\r\n"
				"pubB:\r\n").HexDump(pubB_pkcs1).Add("\r\n"));

		if (pubA_pkcs8 == pubB_pkcs8)
			Console::Out("PKCS#8 public keys are same, OK\r\n");
		else
			Console::Out(Str("PKCS#8 public keys are DIFFERENT:\r\n"
				"pubA:\r\n").HexDump(pubA_pkcs8).Add("\r\n"
				"pubB:\r\n").HexDump(pubB_pkcs8).Add("\r\n"));
	}
	catch (Exception const& e)
	{
		Str msg = "RsaSignerTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
