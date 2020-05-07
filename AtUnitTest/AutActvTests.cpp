#include "AutIncludes.h"


void ActvTests(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	try
	{
		Crypt::Initializer cryptInit;

		BCrypt::Provider* pProvEcdsaP256 = new BCrypt::Provider; // Avoid VS 2015 false positive warning C4702: unreachable code
		Rp<BCrypt::Provider> provEcdsaP256 = pProvEcdsaP256;
		provEcdsaP256->OpenEcdsaP256();

		BCrypt::Provider* pProvSha512 = new BCrypt::Provider; // Avoid VS 2015 false positive warning C4702: unreachable code
		Rp<BCrypt::Provider> provSha512 = pProvSha512;
		provSha512->OpenSha512();

		Str privKey, pubKey;

		{
			Actv::ActvCodeSigner signer { provEcdsaP256, provSha512 };
			signer.GenerateKeypair(privKey, pubKey);
			signer.LoadPrivateKey(privKey);
		}

		Actv::ActvCodeSigner signer { provEcdsaP256, provSha512 };
		signer.LoadPrivateKey(privKey);

		Time now = Time::NonStrictNow();

		Actv::ActvCodeData data;
		data.m_activationCodeFlags = Actv::ActvCodeFlags::AllowActvPings_Https | Actv::ActvCodeFlags::AllowActvPings_Dns;
		data.m_licenseGroupToken = Token::Generate();
		data.m_licenseTypeIdNr = 123;
		data.m_generatedTime = now;
		data.m_licensedToName = "Company with very long name and extra words after it, Inc.";
		data.m_licenseGroupName = "Servers - building 123";
		data.m_groupLicUnitHdsTotal = 100;
		data.m_upgradeExpiryTime = now + Time::FromDays(366);
		data.m_signatureKeyIdNr = 1;

		Str actvCodeBinary;
		signer.Sign(data, actvCodeBinary);

		Str actvCode;
		Base32::Encode(actvCodeBinary, actvCode, Base32::NewLines::None());
		sizet actvCodeLineLen = Actv::ActvCode_BestLineLen(actvCode.Len());

		actvCode.Clear();
		Base32::Encode(actvCodeBinary, actvCode, Base32::NewLines(actvCodeLineLen));

		Str msg = "Activation code:\r\n";
		msg.Add(actvCode).Add("\r\n");
		Console::Out(msg);
		
		Seq actvCodeReader = actvCode;
		Str actvCodeBinary2;
		Base32::Decode(actvCodeReader, actvCodeBinary2);

		Actv::ActvCodeData data2;
		Actv::ActvCodeData::DecodeResult result = data2.Decode(actvCodeBinary2);
		EnsureThrowWithNr(result == Actv::ActvCodeData::DecodeResult::OK, (int64) result);

		EnsureThrow(data == data2);
		EnsureThrow(!(data != data2));
		Console::Out("Decoded data compares OK\r\n");

		++(data.m_signatureKeyIdNr);
		EnsureThrow(!(data == data2));
		EnsureThrow(data != data2);
		Console::Out("Changed data does not compare, OK\r\n");

		Actv::ActvCodeVerifier verifier { provEcdsaP256, provSha512 };
		verifier.LoadPublicKey(pubKey);
		EnsureThrow(verifier.Verify(actvCodeBinary2));
		Console::Out("Signature verifies OK\r\n");
		
		++(actvCodeBinary2[0]);
		EnsureThrow(!verifier.Verify(actvCodeBinary2));
		Console::Out("Corrupted signature does not verify, OK\r\n");

		Console::Out("Testing many signatures");

		sizet nrSigsVerified {}, nrSigsNotVerified {};
		for (sizet i=0; i!=10000; ++i)
		{
			data.m_licenseGroupToken = Token::Generate();

			actvCodeBinary.Clear();
			signer.Sign(data, actvCodeBinary);

			if (verifier.Verify(actvCodeBinary))
				++nrSigsVerified;
			else
				EnsureThrow(!"Signature verification failed");

			++(actvCodeBinary[i % actvCodeBinary.Len()]);
			if (!verifier.Verify(actvCodeBinary))
				++nrSigsNotVerified;
			else
				EnsureThrow(!"Corrupted signature verification succeeded");

			if ((i%1000) == 999)
				Console::Out(".");
		}

		Console::Out(Str("\r\n").UInt(nrSigsVerified).Add(" signatures verified, ").UInt(nrSigsNotVerified).Add(" corrupted sigs not verified: OK\r\n"));
	}
	catch (std::exception const& e)
	{
		Str msg = "ActvTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
