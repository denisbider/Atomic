#include "AutIncludes.h"
#include "AutMain.h"


void DkimTest(int argc, char** argv)
{
	try
	{
		if (argc < 3) throw "Missing DKIM command";

		Seq cmd = argv[2];

		auto loadFileByArg = [&] (int i)
			{
				if (argc <= i) throw "Missing file name";
				Seq fileName = argv[i];
				Str content;
				File().Open(fileName, File::OpenArgs::DefaultRead()).ReadAllInto(content);
				return std::move(content);
			};

		auto loadKeyFileByArg = [&] (int i, RsaSigner* signer, RsaVerifier* verifier)
			{
				if (argc <= i) throw "Missing key file name";
				Str content = loadFileByArg(i);
				Seq reader = content;
				Seq privB64 = reader.ReadToByte(10).Trim();
				Seq pubB64 = reader.DropByte().ReadToByte(10).Trim();
				if (signer)
				{
					Str privBin;
					Base64::MimeDecode(privB64, privBin);
					signer->ImportPriv(privBin);
				}
				if (verifier)
				{
					Str pubBin;
					Base64::MimeDecode(pubB64, pubBin);
					RsaFormat::E fmt = RsaFormat::Any;
					EnsureThrow(verifier->ImportPubPkcs(pubBin, fmt, Asn1::NonCanon::Permit));
					Console::Out(Str("Public key format: ").Add(RsaFormat::GetName(fmt)).Add("\r\n"));
				}
			};

		auto displayInvalidFields = [] (ParseNode const& fieldsNode)
			{
				sizet nrInvalid {};
				for (ParseNode const& fieldNode : fieldsNode)
					if (fieldNode.IsType(Imf::id_invalid_field))
					{
						if (++nrInvalid == 1)
							Console::Out(Str("\r\nInvalid fields under ").Add(fieldsNode.Type().Tag()).Add(":\r\n"));

						Console::Out(fieldNode.SrcText());
					}

				if (nrInvalid)
					Console::Out("\r\n");
				else
					Console::Out(Str("No invalid fields found under ").Add(fieldsNode.Type().Tag()).Add("\r\n"));
			};

		auto parseMsg = [&] (ParseTree& pt)
			{
				pt.RecordBestToStack();
				if (!pt.Parse(Imf::C_message))
				{
					Console::Out(Str().Obj(pt, ParseTree::BestAttempt).Add("\r\n"));
					throw "Message was NOT parsed.";
				}

				Console::Out("Message was parsed.\r\n");

				for (ParseNode const& fieldsNode : pt.Root().FrontFindRef(Imf::id_message_fields))
					displayInvalidFields(fieldsNode);
			};

		Crypt::Initializer cryptInit;

		if (cmd.EqualInsensitive("parse"))
		{
			Str msg = loadFileByArg(3);
			ParseTree pt { msg };
			parseMsg(pt);
		}
		else if (cmd.EqualInsensitive("gen"))
		{
			RsaSigner signer;
			signer.Generate(2048);

			Str privBin, pubBin;
			signer.ExportPriv(privBin).ExportPubPkcs8(pubBin);

			// Encode with Base64
			Str privB64, pubB64;
			Base64::MimeEncode(privBin, privB64, Base64::Padding::Yes, Base64::NewLines::None());
			Base64::MimeEncode(pubBin,  pubB64,  Base64::Padding::Yes, Base64::NewLines::None());

			// Test re-import
			{
				Seq privB64reader = privB64;
				Seq pubB64reader  = pubB64;
				Str privDecoded, pubDecoded;
				Base64::MimeDecode(privB64reader, privDecoded);
				Base64::MimeDecode(pubB64reader,  pubDecoded);
				EnsureThrow(!privB64reader.n);
				EnsureThrow(!pubB64reader.n);
				EnsureThrow(Seq(privDecoded).ConstTimeEqualExact(privBin));
				EnsureThrow(Seq(pubDecoded) .ConstTimeEqualExact(pubBin));

				RsaSigner signer2;
				signer2.ImportPriv(privDecoded);

				RsaVerifier verifier;
				RsaFormat::E fmt = RsaFormat::Pkcs8;
				EnsureThrow(verifier.ImportPubPkcs(pubDecoded, fmt, Asn1::NonCanon::Reject));

				// Try signature and verification
				Str toSign;
				toSign.Chars(1000, 'a');
				Str sigSha1, sigSha256;
				signer2.SignPkcs1_Sha1   (toSign, sigSha1   );
				signer2.SignPkcs1_Sha256 (toSign, sigSha256 );
				EnsureThrow(verifier.VerifyPkcs1_Sha1   (toSign, sigSha1   ));
				EnsureThrow(verifier.VerifyPkcs1_Sha256 (toSign, sigSha256 ));
			}

			// Meant to be redirected into a file which is then read by loadKeyFileByArg()
			Console::Out(Str(privB64).Add("\r\n").Add(pubB64).Add("\r\n"));
		}
		else if (cmd.EqualInsensitive("sign"))
		{
			RsaSigner signer;
			loadKeyFileByArg(3, &signer, nullptr);

			Str msg = loadFileByArg(4);
			Str sigField;
			Dkim::SignMessage(signer, "denisbider.com", "mail", msg, sigField);
			Console::Out(Str(sigField).Add(msg));
		}
		else if (cmd.EqualInsensitive("verify"))
		{
			Str msg = loadFileByArg(4);
			ParseTree pt { msg };
			parseMsg(pt);

			Dkim::SigVerifier verifier { pt };
			verifier.ReadSignatures();
			Console::Out(Str("SigVerifier found ").UInt(verifier.Sigs().Len()).Add(" signatures\r\n"));

			for (Dkim::SigInfo& sig : verifier.Sigs())
				loadKeyFileByArg(3, nullptr, &sig.m_pubKey.Init());

			verifier.VerifySignatures();

			sizet sigNr {};
			for (Dkim::SigInfo const& sig : verifier.Sigs())
			{
				++sigNr;
				Str out;
				out.Set("Signature ").UInt(sigNr).Add(": ").Add(Dkim::SigState::GetName(sig.m_state));
				if (sig.m_state == Dkim::SigState::Failed)
				{
					out.Add(": ").Add(sig.m_failReason)
						.Add("\r\nHeaders signed: ").UInt(sig.m_hdrsSigned.Len());
					if (sig.m_sigInputToVerify.Len())
						out.Add("\r\nsigInputToVerify:\r\n").Add(sig.m_sigInputToVerify);
				}
				out.Add("\r\n");
				Console::Out(out);
			}
		}
		else
			throw "Unrecognized DKIM command";
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Exception:\r\n").Add(e.what()).Add("\r\n"));
	}
}
