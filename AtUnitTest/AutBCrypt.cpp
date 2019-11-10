#include "AutIncludes.h"
#include "AutMain.h"


void BCryptTests(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	try
	{
		BCrypt::Provider* pProvider = new BCrypt::Provider;	// Avoid VS 2015 false positive warning C4702: unreachable code
		Rp<BCrypt::Provider> provider = pProvider;
		provider->OpenEcdhP256();
		Console::Out("Provider opened\r\n");

		BCrypt::Key keyPair;
		keyPair.GenerateKeyPair(provider.Ref(), 0, 0);
		Console::Out("Keypair generated\r\n");

		keyPair.FinalizeKeyPair();
		Console::Out("Keypair finalized\r\n");

		BCrypt::EccPublicBlob pubKeyBlob;
		keyPair.Export(pubKeyBlob.GetBuf(), BCRYPT_ECCPUBLIC_BLOB);
		Console::Out("Public key exported\r\n");

		Console::Out(Str("X: ").Hex(pubKeyBlob.Get_X()).Add("\r\n"));
		Console::Out(Str("Y: ").Hex(pubKeyBlob.Get_Y()).Add("\r\n"));

		BCrypt::Key pubKey;
		pubKey.Import(provider.Ref(), pubKeyBlob.GetBuf(), BCRYPT_ECCPUBLIC_BLOB);
		Console::Out("Public key imported\r\n");
	}
	catch (Exception const& e)
	{
		Str msg = "BCryptTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
