#include "AtIncludes.h"
#include "AtDkim.h"

#include "AtBaseXY.h"
#include "AtImfReadWrite.h"
#include "AtSmtpGrammar.h"


namespace At
{
	namespace Dkim
	{
		using namespace Imf;
		using namespace Mime;


		namespace
		{
			char const* const c_zWs = " \t\r\n";

			void CanonicalizeHeaderValue_Relaxed(Seq value, Enc& enc)
			{
				value.DropToFirstByteNotOf(c_zWs);
				if (value.n)
					while (true)
					{
						enc.Add(value.ReadToFirstByteOf(c_zWs));
						value.DropToFirstByteNotOf(c_zWs);
						if (!value.n)
							break;
						enc.Ch(' ');
					}
			}


			void SignMessage_CollectHeaderFields_Relaxed(Vec<Seq> const& fields, Str& sigField, Str& normFields)
			{
				normFields.Clear().ReserveExact(4000);

				enum { MaxLineLen = 77 };
				sizet sigLineLen = 4;
				sigField.Add(";\r\n  h=");

				Str normName;
				normName.ReserveExact(100);

				sizet colonLen {};
				for (sizet i=fields.Len(); i!=0;)
				{
					Seq field = fields[--i];

					// Read field name, trim whitespace, convert to lowercase
					normName.Clear().Lower(field.ReadToByte(':').Trim());
					field.DropByte();

					// Append canonicalized field name to signature field. Continue on new line if necessary
					if ((sigLineLen >= MaxLineLen / 2) && (sigLineLen + colonLen + normName.Len() > MaxLineLen))
					{
						sigField.Add("\r\n    ");
						sigLineLen = 4;
					}

					if (!colonLen)
						colonLen = 1;
					else
					{
						sigField.Ch(':');
						++sigLineLen;
					}

					sigField.Add(normName);
					sigLineLen += normName.Len();

					// Append to normalized fields
					normFields.Add(normName).Ch(':');
					CanonicalizeHeaderValue_Relaxed(field, normFields);
					normFields.Add("\r\n");
				}
			}


			Seq CanonicalizeMessageBody_Simple(Seq body, Str& storage)
			{
				if (!body.EndsWithExact("\r\n"))
					return storage.Clear().ReserveExact(body.n + 2).Add(body).Add("\r\n");

				while (body.EndsWithExact("\r\n\r\n"))
					body.n -= 2;
				return body;
			}


			Seq CanonicalizeMessageBody_Relaxed(Seq body, Str& storage)
			{
				storage.Clear().ReserveExact(body.n);

				if (body.n)
				{
					do
					{
						Seq line = body.ReadToString("\r\n");
						bool appendWs = false;
						while (line.n)
						{
							Seq chunk = line.ReadToFirstByteOf(c_zWs);
							if (chunk.n)
							{
								if (appendWs)
									storage.Ch(' ');
								storage.Add(chunk);
							}

							line.DropToFirstByteNotOf(c_zWs);
							appendWs = true;
						}

						body.DropBytes(2);
						storage.Add("\r\n");
					}
					while (body.n);

					while (Seq(storage).EndsWithExact("\r\n\r\n"))
						storage.Resize(storage.Len() - 2);
				}

				return storage;
			}

		}	// anon



		// SignMessage

		void SignMessage(RsaSigner& signer, Seq sdid, Seq selector, Seq message, Str& sigField)
		{
			ParseTree pt { message };
			if (!pt.Parse(C_message))
				throw InputErr("DkimSignMessage: Input message could not be parsed");

			ParseNode const& messageFieldsNode = pt.Root().FrontFindRef(id_message_fields);
			ParseNode const& mainFieldsNode = messageFieldsNode.FlatFindRef(id_main_fields);

			struct FieldInfo
			{
				FieldInfo(Ruid const& type) : m_type(type) {}
			
				Ruid const& m_type;
				bool        m_present {};
			};

			FieldInfo signFields[] =
				{	id_orig_date, id_from, id_sender, id_reply_to,															// Imf::C_origin_field
					id_to, id_cc, id_bcc,																					// Imf::C_dest_field
					id_message_id, id_in_reply_to, id_references,															// Imf::C_id_field
					id_subject, id_comments, id_keywords,																	// Imf::C_info_field
					id_mime_version, id_content_type, id_content_enc, id_content_id, id_content_desc, id_content_disp };	// Mime::C_msg_header_field
			
			Vec<Seq> fieldsToSign;
			fieldsToSign.ReserveExact(20);

			for (ParseNode const& c : mainFieldsNode)
				for (FieldInfo& fi : signFields)
					if (c.IsType(fi.m_type))
					{
						fi.m_present = true;
						fieldsToSign.Add(c.SrcText());
					}

			sigField.Clear().ReserveExact(1000)
				.Add("DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed;\r\n"
					 "  d=").Add(sdid).Add("; s=").Add(selector);

			Str normFields;
			SignMessage_CollectHeaderFields_Relaxed(fieldsToSign, sigField, normFields);

			Seq body;
			ParseNode const* bodyNode = pt.Root().FirstChild().FlatFind(id_body);
			if (bodyNode)
				body = bodyNode->SrcText();

			Str normBodyStorage;
			Seq normBody = CanonicalizeMessageBody_Relaxed(body, normBodyStorage);

			Str bh = Hash::HashOf(normBody, CALG_SHA_256);
			sigField.Add(";\r\n  bh=");
			Base64::MimeEncode(bh, sigField, Base64::Padding::Yes, Base64::NewLines::None());
			sigField.Add(";\r\n  b=");

			normFields.Add("dkim-signature:");
			CanonicalizeHeaderValue_Relaxed(Seq(sigField).DropToByte(':').DropByte(), normFields);

			Str sig;
			signer.SignPkcs1_Sha256(normFields, sig);

			Base64::MimeEncode(sig, sigField, Base64::Padding::Yes, Base64::NewLines(78, 4, "\r\n    "));
			sigField.Add("\r\n");
		}



		// SigState

		char const* SigState::GetName(uint n)
		{
			switch (n)
			{
			case None:     return "None";
			case Failed:   return "Failed";
			case Parsed:   return "Parsed";
			case Verified: return "Verified";
			default:       return "<unrecognized>";
			}
		}



		// SigVerifier

		void SigVerifier::ReadSignatures()
		{
			m_sigs.Clear();

			ParseNode const& msgFieldsNode = m_pt.Root().FrontFindRef(id_message_fields);
			for (ParseNode const& fieldsNode : msgFieldsNode)
				for (ParseNode const& fieldNode : fieldsNode)
					if (fieldNode.IsType(id_dkim_signature))
						ReadSignature(msgFieldsNode, fieldNode);
		}


		bool SigVerifier::ReadSignature(ParseNode const& msgFieldsNode, ParseNode const& sigNode)
		{
			SigInfo& sig = m_sigs.Add();
			Seq encodedSigValue;

			ParseNode const& tagListNode = sigNode.FlatFindRef(id_dkim_tag_list);
			bool have_v{}, have_a{}, have_b{}, have_bh{}, have_c{}, have_d{}, have_h{}, have_i{}, have_l{}, have_q{}, have_s{}, have_t{}, have_x{};

			for (ParseNode const& tagListNodeChild : tagListNode)
				if (tagListNodeChild.IsType(id_dkim_tag))
				{
					Seq name, value;
					for (ParseNode const& kv : tagListNodeChild)
						if (kv.IsType(id_dkim_tag_name))
							name = kv.SrcText();
						else if (kv.IsType(id_dkim_tag_value))
						{
							value = kv.SrcText();
							break;
						}

					if (name == "v")
					{
						if (!have_v) have_v = true; else return sig.SetFail("Duplicate v= tag");
						if (value != "1") return sig.SetFail("Unrecognized value of version tag");
					}
					else if (name == "a")
					{
						if (!have_a) have_a = true; else return sig.SetFail("Duplicate a= tag");

						     if (value == "rsa-sha1"   ) sig.m_alg = SigAlg::RsaSha1;
						else if (value == "rsa-sha256" ) sig.m_alg = SigAlg::RsaSha256;
						else return sig.SetFail("Unrecognized signature algorithm");
					}
					else if (name == "b")
					{
						if (!have_b) have_b = true; else return sig.SetFail("Duplicate b= tag");
						
						encodedSigValue = value;
						
						Base64::MimeDecode(value, sig.m_signatureEncoded);
						if (value.n) return sig.SetFail("Unrecognized data in b= tag");
					}
					else if (name == "bh")
					{
						if (!have_bh) have_bh = true; else return sig.SetFail("Duplicate bh= tag");

						Base64::MimeDecode(value, sig.m_bodyHashEncoded);
						if (value.n) return sig.SetFail("Unrecognized data in bh= tag");
					}
					else if (name == "c")
					{
						if (!have_c) have_c = true; else return sig.SetFail("Duplicate c= tag");
						
						Seq first = value.ReadToByte('/');
						     if (first == "relaxed") sig.m_hdrCanon = SigCanon::Relaxed;
						else if (first != "simple" ) return sig.SetFail("Unrecognized header canonicalization algorithm");

						if (value.n)
						{
							value.DropByte();
							     if (value == "relaxed") sig.m_bodyCanon = SigCanon::Relaxed;
							else if (value != "simple" ) return sig.SetFail("Unrecognized body canonicalization algorithm");
						}
					}
					else if (name == "d")
					{
						if (!have_d) have_d = true; else return sig.SetFail("Duplicate d= tag");

						// RFC 6376 specifies the use of stricter, less nonsense SMTP domain syntax (RFC 5321).
						// The IMF domain syntax (RFC 5322) permits domain literals, and also comments and folding whitespace between domain components.
						ParseTree pt { value };
						if (!pt.Parse(Smtp::C_Domain))
							return sig.SetFail("SDID is not a valid domain name");

						sig.m_sdid = value;
					}
					else if (name == "h")
					{
						if (!have_h) have_h = true; else return sig.SetFail("Duplicate h= tag");

						ParseTree pt { value };
						if (!pt.Parse(C_dkim_hdr_list))
							return sig.SetFail("Invalid signed header list format");

						for (ParseNode const& hdrListNodeChild : pt.Root().FirstChild())
							if (hdrListNodeChild.IsType(id_field_name))
								sig.m_hdrsSigned.Add(hdrListNodeChild.SrcText());
					}
					else if (name == "i")
					{
						if (!have_i) have_i = true; else return sig.SetFail("Duplicate i= tag");

						ParseTree pt { value };
						if (!pt.Parse(C_dkim_auid))
							return sig.SetFail("Unrecognized AUID format");

						sig.m_auid.Init(value);
					}
					else if (name == "l")
					{
						if (!have_l) have_l = true; else return sig.SetFail("Duplicate l= tag");

						uint64 bodyLen = value.ReadNrUInt64Dec();
						if (value.n || bodyLen == UINT64_MAX)
							return sig.SetFail("Invalid signed body length");

						sig.m_bodyLenSigned.Init(bodyLen);
					}
					else if (name == "q")
					{
						if (!have_q) have_q = true; else return sig.SetFail("Duplicate q= tag");
						if (value != "dns/txt") return sig.SetFail("Invalid query method");
					}
					else if (name == "s")
					{
						if (!have_s) have_s = true; else return sig.SetFail("Duplicate s= tag");

						ParseTree pt { value };
						if (!pt.Parse(Smtp::C_Domain))
							return sig.SetFail("Selector is not a valid domain name");

						sig.m_selector = value;
					}
					else if (name == "t")
					{
						if (!have_t) have_t = true; else return sig.SetFail("Duplicate t= tag");
						
						uint64 seconds = value.ReadNrUInt64Dec();
						if (value.n || seconds > (uint64) INT64_MAX)
							return sig.SetFail("Invalid signature timestamp");

						sig.m_timestamp.Init(Time::FromUnixTime((int64) seconds));
					}
					else if (name == "x")
					{
						if (!have_x) have_x = true; else return sig.SetFail("Duplicate x= tag");
						
						uint64 seconds = value.ReadNrUInt64Dec();
						if (value.n || seconds > (uint64) INT64_MAX)
							return sig.SetFail("Invalid signature expiration");

						sig.m_expiration.Init(Time::FromUnixTime((int64) seconds));
					}
				}

			if (!have_v  ) return sig.SetFail("Missing v= tag"  );
			if (!have_a  ) return sig.SetFail("Missing a= tag"  );
			if (!have_b  ) return sig.SetFail("Missing b= tag"  );
			if (!have_bh ) return sig.SetFail("Missing bh= tag" );
			if (!have_d  ) return sig.SetFail("Missing d= tag"  );
			if (!have_h  ) return sig.SetFail("Missing h= tag"  );
			if (!have_s  ) return sig.SetFail("Missing s= tag"  );
			
			if ((sig.m_alg == SigAlg::RsaSha1   && sig.m_bodyHashEncoded.Len() != 20) ||
				(sig.m_alg == SigAlg::RsaSha256 && sig.m_bodyHashEncoded.Len() != 32))
				return sig.SetFail("Encoded body hash size does not match algorithm");

			if (!CalculateBodyHash(sig))
				return false;
			if (!Seq(sig.m_bodyHashCalculated).ConstTimeEqualExact(sig.m_bodyHashEncoded))
				return sig.SetFail("Encoded body hash does not match calculated");

			ConstructSigInput(msgFieldsNode, sigNode, sig, encodedSigValue);
			sig.m_state = SigState::Parsed;
			return true;
		}


		bool SigVerifier::CalculateBodyHash(SigInfo& sig)
		{
			// Canonicalize message body
			Seq origBody;
			ParseNode const* bodyNode = m_pt.Root().FirstChild().FlatFind(id_body);
			if (bodyNode)
				origBody = bodyNode->SrcText();

			Str canonBodyStorage;
			Seq canonBody;
			     if (sig.m_bodyCanon == SigCanon::Simple  ) canonBody = CanonicalizeMessageBody_Simple  (origBody, canonBodyStorage);
			else if (sig.m_bodyCanon == SigCanon::Relaxed ) canonBody = CanonicalizeMessageBody_Relaxed (origBody, canonBodyStorage);
			else EnsureThrow(!"Unexpected body canonicalization algorithm");

			// Determine how much of the canonicalized body to sign
			Seq canonBodyReader = canonBody;
			Seq bytesToHash;
			if (!sig.m_bodyLenSigned.Any())
				bytesToHash = canonBodyReader.ReadBytes(SIZE_MAX);
			else
			{
				uint64 lenSigned64 = sig.m_bodyLenSigned.Ref();
				if (lenSigned64 > canonBodyReader.n) return sig.SetFail("Specified body length exceeds canonicalized body size");
				bytesToHash = canonBodyReader.ReadBytes((sizet) lenSigned64);
			}

			sig.m_bodyLenUnsigned = canonBodyReader.n;

			// Hash canonicalized body
			ALG_ID algId {};
			     if (sig.m_alg == SigAlg::RsaSha1   ) algId = CALG_SHA1;
			else if (sig.m_alg == SigAlg::RsaSha256 ) algId = CALG_SHA_256;
			else EnsureThrow(!"Unexpected signature algorithm");

			sig.m_bodyHashCalculated = Hash::HashOf(bytesToHash, algId);
			return true;
		}


		void SigVerifier::ConstructSigInput(ParseNode const& msgFieldsNode, ParseNode const& sigNode, SigInfo& sig, Seq encodedSigValue)
		{
			// Build list of fields in message
			struct FieldInfo { Seq m_name, m_field; };
			Vec<FieldInfo> fields;

			for (ParseNode const& fieldsNode : msgFieldsNode)
				for (ParseNode const& fieldNode : fieldsNode)
				{
					FieldInfo& fi = fields.Add();
					fi.m_name = fieldNode.FrontFindRef(id_field_name).SrcText();
					fi.m_field = fieldNode.SrcText();
				}

			// Collect and canonicalize header fields
			sig.m_sigInputToVerify.Clear().ReserveExact(msgFieldsNode.SrcText().n);

			for (Seq fieldName : sig.m_hdrsSigned)
			{
				// Search fields by name starting with last. If field not found, ignore (it is signed as a null input)
				for (sizet i=fields.Len(); i!=0; )
				{
					FieldInfo& fi = fields[--i];
					if (fi.m_name.n && fi.m_name.EqualInsensitive(fieldName))
					{
						AddSigInputField(sig, fi.m_field);
						sig.m_sigInputToVerify.Add("\r\n");

						// Clear the field so it will not be found if referenced again (but any earlier field of same name will be found)
						fi.m_name = Seq();
						fi.m_field = Seq();
						break;
					}
				}
			}

			// Construct and append a version of the DKIM-Signature field with the signature value removed
			Seq origSigField = sigNode.SrcText();
			EnsureThrow(origSigField.p < encodedSigValue.p);
			EnsureThrow(origSigField.p + origSigField.n >= encodedSigValue.p + encodedSigValue.n);

			Seq sigFieldHalf1 { origSigField.p, (sizet) (encodedSigValue.p - origSigField.p) };
			Seq sigFieldHalf2 { encodedSigValue.p + encodedSigValue.n, origSigField.n - (sigFieldHalf1.n + encodedSigValue.n) };
			EnsureThrow(sigFieldHalf1.n + encodedSigValue.n + sigFieldHalf2.n == origSigField.n);

			Str verifySigField;
			verifySigField.ReserveExact(sigFieldHalf1.n + sigFieldHalf2.n).Add(sigFieldHalf1).Add(sigFieldHalf2);
			AddSigInputField(sig, verifySigField);
		}


		void SigVerifier::AddSigInputField(SigInfo& sig, Seq field)
		{
			if (sig.m_hdrCanon == SigCanon::Simple)
				sig.m_sigInputToVerify.Add(field);
			else
			{
				sig.m_sigInputToVerify.Lower(field.ReadToByte(':')).Ch(':');
				field.DropByte();
				CanonicalizeHeaderValue_Relaxed(field, sig.m_sigInputToVerify);
			}
		}


		void SigVerifier::VerifySignatures()
		{
			for (SigInfo& sig : m_sigs)
				if (sig.m_state == SigState::Parsed && sig.m_pubKey.Any())
				{
					bool ok {};
						 if (sig.m_alg == SigAlg::RsaSha1   ) ok = sig.m_pubKey.Ref().VerifyPkcs1_Sha1   (sig.m_sigInputToVerify, sig.m_signatureEncoded);
					else if (sig.m_alg == SigAlg::RsaSha256 ) ok = sig.m_pubKey.Ref().VerifyPkcs1_Sha256 (sig.m_sigInputToVerify, sig.m_signatureEncoded);
					else EnsureThrow(!"Unexpected signature algorithm");

					if (ok)
						sig.m_state = SigState::Verified;
					else
						sig.SetFail("Signature verification failed");
				}
		}

	}
}
