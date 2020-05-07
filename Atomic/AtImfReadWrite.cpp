#include "AtIncludes.h"
#include "AtImfReadWrite.h"

#include "AtBaseXY.h"
#include "AtCharsets.h"
#include "AtTime.h"


namespace At
{
	namespace Imf
	{

		// Strings

		namespace
		{
			byte* ReadEncodedWord_QP(Seq encoded, byte* pWrite)
			{
				uint c;
				while ((c = encoded.ReadByte()) != UINT_MAX)
				{
					if (c == '_')
						*pWrite++ = ' ';
					else if (c != '=')
						*pWrite++ = (byte) c;
					else
					{
						c = encoded.ReadHexEncodedByte();
						if (c == UINT_MAX)
							*pWrite++ = '=';
						else
							*pWrite++ = (byte) c;
					}
				}

				return pWrite;
			}


			Seq ReadEncodedWord_QP(Seq encoded, Enc& out)
			{
				return out.InvokeWriter(encoded.n, [encoded] (byte* pStart) -> byte*
					{ return ReadEncodedWord_QP(encoded, pStart); } );
			}


			byte* ReadEncodedWord(ParseNode const& encwNode, byte* pWrite, byte const* pEnd, AuxStorage& storage)
			{
				EnsureThrow(encwNode.IsType(id_encoded_word));

				Seq charset;
				Seq encoding;
				Seq encoded;

				for (ParseNode const& c : encwNode)
				{
						 if (c.IsType(id_encw_charset) ) charset  = c.SrcText();
					else if (c.IsType(id_encw_encoding)) encoding = c.SrcText();
					else if (c.IsType(id_encoded_text) ) encoded  = c.SrcText();
				}

				if (!encoded.n)
					return pWrite;

				UINT cp = CharsetNameToWindowsCodePage(charset);
				if (cp == CP_UTF8)
				{
					if (encoding.EqualInsensitive("q"))
						return ReadEncodedWord_QP(encoded, pWrite);
					if (encoding.EqualInsensitive("b"))
						return Base64::MimeDecode(encoded, pWrite);
				}
				else if (cp)
				{
					AuxStr decodedHolder { storage };
					Seq decoded;

					if (encoding.EqualInsensitive("q"))
						decoded = ReadEncodedWord_QP(encoded, decodedHolder);
					else if (encoding.EqualInsensitive("b"))
						decoded = Base64::MimeDecode(encoded, decodedHolder);

					if (decoded.n)
					{
						// Convert input code page to UTF-16
						int cchMultiByte { NumCast<int>(decoded.n) };
						int rc { MultiByteToWideChar(cp, 0, (LPCSTR) decoded.p, cchMultiByte, 0, 0) };
						if (rc > 0)
						{
							int cchWideChar { rc + 4 };
							AuxStr wideHolder { storage };
							wideHolder.Resize(sizeof(wchar_t) * cchWideChar);

							LPWSTR pWide { (LPWSTR) wideHolder.Ptr() };
							rc = MultiByteToWideChar(cp, 0, (LPCSTR) decoded.p, cchMultiByte, pWide, cchWideChar);
							if (rc > 0)
							{
								// Convert UTF-16 to UTF-8
								cchWideChar = rc;
								rc = WideCharToMultiByte(CP_UTF8, 0, pWide, cchWideChar, 0, 0, 0, 0);
								if (rc > 0)
								{
									int cbUtf8 { rc + 4 };
									EnsureThrow(pWrite + cbUtf8 <= pEnd);
									rc = WideCharToMultiByte(CP_UTF8, 0, pWide, cchWideChar, (LPSTR) pWrite, cbUtf8, 0, 0);
									if (rc > 0)
									{
										// Converted
										EnsureAbort(pWrite + rc <= pEnd);
										return pWrite += rc;
									}
								}
							}
						}
					}
				}

				// We were unsuccessful. Copy whole original node
				Seq src = encwNode.SrcText();
				memcpy(pWrite, src.p, src.n);
				return pWrite + src.n;
			}

		
			byte* ReadEncodedWordGroup(ParseNode const& encwGroupNode, byte* pWrite, byte const* pEnd, AuxStorage& storage)
			{
				EnsureThrow(encwGroupNode.IsType(id_encw_group));
				for (ParseNode const& c : encwGroupNode)
					if (c.IsType(id_encoded_word))
						pWrite = ReadEncodedWord(c, pWrite, pEnd, storage);

				return pWrite;
			}

		} // anon


		namespace
		{
			byte* ReadQuotedStringContent(ParseNode const& contentNode, byte* pWrite)
			{
				auto addByte = [&] (byte c) { *pWrite++ = c; };
				auto addSeq  = [&] (Seq  s) { memcpy(pWrite, s.p, s.n); pWrite += s.n; };

				for (ParseNode const& c : contentNode)
				{
					if (c.IsType(id_FWS))
					{
						// Drop any CR, LF, or combo of them. Leave any other whitespace 
						Seq reader { c.SrcText() };
						while (reader.n)
						{
							Seq ws = reader.ReadToFirstByteOf("\r\n");
							if (ws.n != 0)
								addSeq(ws);

							reader.DropToFirstByteNotOf("\r\n");
						}
					}
					else if (c.IsType(id_quoted_pair))
					{
						// Unescape
						EnsureThrow(c.SrcText().n == 2);
						addByte(c.SrcText().p[1]);
					}
					else
					{
						// Copy verbatim
						EnsureThrow(c.IsType(id_qtext_word));
						addSeq(c.SrcText());
					}
				}

				return pWrite;
			}

			bool ForEachWord(Seq s, std::function<void(Seq word, bool isFirst, bool isLast)> onWord)
			{
				Seq reader { s.Trim() };
				if (!reader.n)
					return false;

				Seq firstWord { reader.ReadToFirstByteOf(Seq::AsciiWsBytes) };
				if (!reader.n)
				{
					// No whitespace in string - single word
					onWord(firstWord, true, true);
				}
				else
				{
					// String has at least two words separated by whitespace
					onWord(firstWord, true, false);
					reader.DropToFirstByteNotOf(Seq::AsciiWsBytes);

					// Write out the mid words, if any
					Seq mid { reader.ReadToAfterLastByteOf(Seq::AsciiWsBytes) };
					while (mid.n)
					{
						Seq midWord = mid.ReadToFirstByteOf(Seq::AsciiWsBytes);
						mid.DropToFirstByteNotOf(Seq::AsciiWsBytes);
						onWord(midWord, false, false);
					}

					// Last word
					onWord(reader, false, true);
				}

				return true;
			}
		
		}  // anon


		Seq ReadQuotedString(ParseNode const& qsNode, PinStore& store)
		{
			EnsureThrow(qsNode.IsType(id_quoted_string));
			ParseNode const* contentNode { qsNode.FlatFind(id_qs_content) };
			if (!contentNode)
				return Seq();

			Enc::Write write = store.IncWrite(contentNode->SrcText().n);
			byte* pStart = write.Ptr();
			byte* pWrite = ReadQuotedStringContent(*contentNode, pStart);
			return Seq(pStart, write.AddUpTo(pWrite));
		}

		void WriteQuotedString(MsgWriter& writer, Seq value, Seq afterClose)
		{
			bool anyWords = ForEachWord(value, [&] (Seq word, bool isFirst, bool isLast)
				{
					MsgSectionScope section { writer };
					
					if (isFirst)
						writer.Add("\"");
					else
						writer.Add(" ");

					while (word.n)
					{
						// Individual UTF-8 character bytes pass through as qtext chars and are not escaped.
						// As long as input UTF-8 is valid, its validity in output is preserved.

						Seq noEsc { word.ReadToFirstByteNotOfType(Is_std_qtext_char) };
						writer.Add(noEsc);

						Seq esc { word.ReadToFirstByteOfType(Is_std_qtext_char) };
						while (esc.n)
							writer.Add("\\").Add(esc.ReadBytes(1));
					}

					if (isLast)
					{
						writer.Add("\"");
						if (afterClose.n)
							writer.Add(afterClose);
					}
				} );

			if (!anyWords)
			{
				MsgSectionScope section { writer };
				writer.Add("\"\"");
				if (afterClose.n)
					writer.Add(afterClose);
			}
		}


		Seq ReadPhrase(ParseNode const& phraseNode, PinStore& store)
		{
			EnsureThrow(phraseNode.IsType(id_phrase));

			// An encoded word could use a tricksy code page that produces multiple UTF8 bytes per input byte.
			sizet       maxLen    = Utf8::MaxBytesPerChar * phraseNode.SrcText().n;
			Enc::Write  write     = store.IncWrite(maxLen);
			byte*       pWrite    = write.Ptr();
			byte const* pStart    = pWrite;
			byte const* pEnd      = pWrite + maxLen;
			bool        skippedWs {};

			auto addByte = [&] (byte c) { *pWrite++ = c; };
			auto addSeq  = [&] (Seq  s) { memcpy(pWrite, s.p, s.n); pWrite += s.n; };
			auto onText  = [&] ()       { if (skippedWs) { if (pStart != pWrite) { addByte(' '); } skippedWs = false; } };

			for (ParseNode const& c : phraseNode)
			{
				if (c.IsType(id_CFWS))
				{
					// Leading and trailing whitespace is ignored. Whitespace in middle gets converted to single space.
					// Whitespace is added in onText, when we actually encounter text.
					skippedWs = true;
				}
				else if (c.IsType(id_atext_word) || c.IsType(Parse::id_Dot))
				{
					onText();
					addSeq(c.SrcText());
				}
				else if (c.IsType(id_quoted_string))
				{
					ParseNode const* contentNode = c.FlatFind(id_qs_content);
					if (contentNode != nullptr)
					{
						// Quoted string with content
						onText();
						pWrite = ReadQuotedStringContent(*contentNode, pWrite);
					}

					skippedWs = true;
				}
				else if (c.IsType(id_encw_group))
				{
					onText();
					pWrite = ReadEncodedWordGroup(c, pWrite, pEnd, store);
				}
				else
					throw InputErr("Unexpected parser node type within phrase");
			}

			return Seq(pStart, write.AddUpTo(pWrite));
		}


		namespace
		{
			bool ContainsHighBitOrCtlBytes(Seq value)
			{
				value.ReadToFirstByteOfType( [] (uint c) -> bool { return c>126 || (c<32 && !ZChr(Seq::AsciiWsBytes, c)); } );
				return value.n != 0;
			}

			void WriteEncodedWordsUtf8(MsgWriter& writer, Seq value)
			{
				Str normalized;
				normalized.ReserveExact(value.n);

				ForEachWord(value, [&] (Seq word, bool isFirst, bool)
					{
						if (!isFirst)
							normalized.Ch(' ');
						normalized.Add(word);
					} );

				// Per RFC 2047, maximum encoded-word length is 75 characters.
				// When using UTF-8, twelve characters are consumed by prefix: "=?utf-8?b?", and suffix: "?="
				// 63 characters remain, which fit 15.75 base64 chunks of 3 input bytes each, converted to 4 bytes in output
				// Rounded down, this is 15 base64 chunks, for max 45 bytes of input per encoded-word

				Seq reader(normalized);
				bool first { true };
				while (reader.n)
				{
					MsgSectionScope section { writer, 75 };

					if (first)
						first = false;
					else
						writer.Add(" ");

					Seq encWordPlain { reader.ReadUtf8_MaxBytes(45) };
					AuxStr auxStr { writer };
					writer.Add("=?utf-8?b?").Add(Base64::MimeEncode(encWordPlain, auxStr, Base64::Padding::Yes, Base64::NewLines::Mime())).Add("?=");
				}
			}

			void WritePlainWords(MsgWriter& writer, Seq value)
			{
				ForEachWord(value, [&] (Seq word, bool isFirst, bool)
					{
						MsgSectionScope section { writer };
						if (!isFirst)
							writer.Add(" ");
						writer.Add(word);
					} );
			}

		} // anon


		void WritePhrase(MsgWriter& writer, Seq value)
		{
			// We use three ways to encode a phrase:
			// - encoded-word + UTF-8 + Base64 if input contains any high-bit characters or control characters other than whitespace
			// - quoted-string if input contains no such characters, but is not parsable as Imf::id_phrase
			// - the phrase verbatim if it's already parsable as Imf::id_phrase
			//
			// In each case, for consistency in output, we use ForEachWord() to tokenize the phrase and normalize whitespace.
			// Leading and trailing whitespace is stripped. Intervening whitespace is replaced with a single space.
			// If an input can be encoded using two or more of the three methods, the result after decoding should be identical,
			// regardless of which encoding method is chosen.
			//
			// Implementation is very similar to WriteUnstructured().

			if (ContainsHighBitOrCtlBytes(value))
				WriteEncodedWordsUtf8(writer, value);
			else
			{
				ParseTree pt { value };
				if (!pt.Parse(C_phrase))
					WriteQuotedString(writer, value);
				else
					WritePlainWords(writer, value);
			}
		}


		Seq ReadDomain(ParseNode const& domainNode, PinStore& store)
		{
			EnsureThrow(domainNode.IsType(id_domain));

			sizet       maxLen  = domainNode.SrcText().n;
			Enc::Write  write   = store.IncWrite(maxLen);
			byte*       pWrite  = write.Ptr();
			byte const* pStart  = pWrite;

			auto addByte = [&] (byte c) { *pWrite++ = c; };
			auto addSeq  = [&] (Seq  s) { memcpy(pWrite, s.p, s.n); pWrite += s.n; };

			for (ParseNode const& c : domainNode)
			{
				if (c.IsType(id_CFWS) ||
					c.IsType(id_FWS))
				{
					// Drop
				}
				else if (c.IsType(id_dot_atom_text)   ||
						 c.IsType(id_atext_word)      ||
						 c.IsType(Parse::id_Dot)      ||
						 c.IsType(Parse::id_SqOpenBr) ||
						 c.IsType(id_std_dtext)       ||
						 c.IsType(id_obs_dtext_char)  ||
						 c.IsType(Parse::id_SqCloseBr))
				{
					// Copy verbatim
					addSeq(c.SrcText());
				}
				else if (c.IsType(id_quoted_pair))
				{
					// Unescape
					EnsureThrow(c.SrcText().n == 2);
					addByte(c.SrcText().p[1]);
				}
				else
					throw InputErr("Unexpected parser node type within domain");
			}

			return Seq(pStart, write.AddUpTo(pWrite));
		}


		Seq ReadUnstructured(ParseNode const& unstrNode, PinStore& store)
		{
			EnsureThrow(unstrNode.IsType(id_unstructured));

			// An encoded word could use a tricksy code page that produces multiple UTF8 bytes per input byte.
			sizet       maxLen    = Utf8::MaxBytesPerChar * unstrNode.SrcText().n;
			Enc::Write  write     = store.IncWrite(maxLen);
			byte*       pWrite    = write.Ptr();
			byte const* pStart    = pWrite;
			byte const* pEnd      = pWrite + maxLen;
			bool        skippedWs {};

			auto addByte = [&] (byte c) { *pWrite++ = c; };
			auto addSeq  = [&] (Seq  s) { memcpy(pWrite, s.p, s.n); pWrite += s.n; };
			auto onText  = [&] ()       { if (skippedWs) { if (pStart != pWrite) { addByte(' '); } skippedWs = false; } };

			for (ParseNode const& c : unstrNode)
			{
				if (c.IsType(id_FWS) || c.IsType(id_ImfWs))
				{
					// Leading and trailing whitespace is ignored. Whitespace in middle gets converted to single space.
					// Whitespace is added in onText, when we actually encounter text.
					skippedWs = true;
				}
				else if (c.IsType(id_utext_word))
				{
					onText();
					addSeq(c.SrcText());
				}
				else if (c.IsType(id_encw_group))
				{
					onText();
					pWrite = ReadEncodedWordGroup(c, pWrite, pEnd, store);
				}
				else
					throw InputErr("Unexpected parser node type within unstructured text");
			}

			return Seq(pStart, write.AddUpTo(pWrite));
		}


		void WriteUnstructured(MsgWriter& writer, Seq value)
		{
			// We use two ways to encode unstructured text:
			// - encoded-word + UTF-8 + Base64 if input contains any high-bit characters or control characters other than whitespace
			// - the unstructured text verbatim
			//
			// In each case, for consistency in output, we use ForEachWord() to tokenize the text and normalize whitespace.
			// Leading and trailing whitespace is stripped. Intervening whitespace is replaced with a single space.
			// If an input can be encoded using either method, decoded result should be identical, regardless of method chosen.
			//
			// Implementation is very similar to WritePhrase().

			if (ContainsHighBitOrCtlBytes(value))
				WriteEncodedWordsUtf8(writer, value);
			else
				WritePlainWords(writer, value);
		}



		// Addresses

		void AddrSpec::Read(ParseNode const& addrSpecNode, PinStore& store)
		{
			EnsureThrow(addrSpecNode.IsType(id_addr_spec));

			// Local part
			ParseNode const& localPartNode { addrSpecNode.FirstChild() };
			EnsureThrow(localPartNode.IsType(id_local_part));

			// RFC 5321: "due to a long history of problems when intermediate hosts have attempted to optimize transport by modifying them,
			// the local-part MUST be interpreted and assigned semantics only by the host specified in the domain part of the address."
			m_localPart = localPartNode.SrcText().Trim();

			// Domain
			ParseNode const& domainNode { addrSpecNode.LastChild() };
			EnsureThrow(domainNode.IsType(id_domain));

			m_domain = ReadDomain(domainNode, store);
		}

		void AddrSpec::Write(MsgWriter& writer) const
		{
			MsgSectionScope section { writer };
			writer.Add(m_localPart).Add("@").Add(m_domain);
		}


		void Mailbox::Read(ParseNode const& mailboxNode, PinStore& store)
		{
			EnsureThrow(mailboxNode.IsType(id_mailbox));

			ParseNode const* phraseNode { mailboxNode.FrontFind(id_phrase) };
			if (phraseNode != nullptr)
				m_name.ReInit().Read(*phraseNode, store);

			ParseNode const& addrSpecNode { mailboxNode.DeepFindRef(id_addr_spec) };
			m_addr.Read(addrSpecNode, store);
		}

		void Mailbox::Write(MsgWriter& writer) const
		{
			if (!m_name.Any())
				m_addr.Write(writer);
			else
			{
				m_name->Write(writer);
				writer.Add(" ");

				MsgSectionScope section { writer };
				writer.Add("<");
				m_addr.Write(writer);
				writer.Add(">");
			}
		}


		void Group::Read(ParseNode const& groupNode, PinStore& store)
		{
			EnsureThrow(groupNode.IsType(id_group));

			ParseNode const& phraseNode { groupNode.FrontFindRef(id_phrase) };
			m_name = ReadPhrase(phraseNode, store);

			ParseNode const* mailboxListNode { groupNode.FlatFind(id_mailbox_list) };
			if (mailboxListNode != nullptr)
			{
				for (ParseNode const& c : *mailboxListNode)
					if (c.IsType(id_mailbox))
						m_mboxes.Add().Read(c, store);
			}
		}

		void Group::Write(MsgWriter& writer) const
		{
			WritePhrase(writer, m_name);
			writer.Add(":");
			if (m_mboxes.Any())
			{
				m_mboxes.First().Write(writer);
				for (Mailbox const& mbox : m_mboxes.GetSlice(1))
				{
					writer.Add(", ");
					mbox.Write(writer);
				}
			}
			writer.Add(";");
		}


		// Address

		void Address::Read(ParseNode const& addressNode, PinStore& store)
		{
			EnsureThrow(addressNode.IsType(id_address));

			ParseNode const& fc { addressNode.FirstChild() };
			if (fc.IsType(id_mailbox))
			{
				m_type = Type::Mailbox;
				m_mbox.Read(fc, store);
			}
			else
			{
				m_type = Type::Group;
				m_group.Read(fc, store);
			}
		}


		void Address::Write(MsgWriter& writer) const
		{
			if (m_type == Type::Mailbox)
				m_mbox.Write(writer);
			else if (m_type == Type::Group)
				m_group.Write(writer);
		}


		sizet Address::ExtractAddrSpecs(Vec<AddrSpec>& addrSpecs) const
		{
			sizet n {};

			switch (m_type)
			{
			case Address::Type::Mailbox:
				addrSpecs.Add(m_mbox.m_addr);
				++n;
				break;

			case Address::Type::Group:
				for (Mailbox const& m : m_group.m_mboxes)
				{
					addrSpecs.Add(m.m_addr);
					++n;
				}
				break;

			case Address::Type::None:
				break;
			}

			return n;
		}



		// AddressList

		void AddressList::Read(ParseNode const& addressListNode, PinStore& store)
		{
			EnsureThrow(addressListNode.IsType(id_address_list));

			for (ParseNode const& c : addressListNode)
				if (c.IsType(id_address))
					m_addresses.Add().Read(c, store);
		}


		void AddressList::Write(MsgWriter& writer) const
		{
			if (m_addresses.Any())
			{
				m_addresses.First().Write(writer);

				for (Address const& a : m_addresses.GetSlice(1))
				{
					writer.Add(", ");
					a.Write(writer);
				}
			}
		}


		sizet AddressList::ExtractAddrSpecs(Vec<AddrSpec>& addrSpecs) const
		{
			sizet n {};
			for (Address const& a : m_addresses)
				n += a.ExtractAddrSpecs(addrSpecs);
			return n;
		}



		// Message

		void DateField::Read(ParseNode const& origDateNode, PinStore&)
		{
			EnsureThrow(origDateNode.IsType(id_orig_date));
			m_dt.Read(origDateNode.FlatFindRef(id_date_time));
		}

		void DateField::Write(MsgWriter& writer) const
		{
			writer.Add("Date: ");
			m_dt.Write(writer);
			writer.Add("\r\n");
		}


		void FromField::Read(ParseNode const& fromNode, PinStore& store)
		{
			EnsureThrow(fromNode.IsType(id_from));
			m_addrList.Read(fromNode.FlatFindRef(id_address_list), store);
		}

		void FromField::Write(MsgWriter& writer) const
		{
			writer.Add("From: ");
			m_addrList.Write(writer);
			writer.Add("\r\n");
		}


		void SenderField::Read(ParseNode const& senderNode, PinStore& store)
		{
			EnsureThrow(senderNode.IsType(id_sender));
			m_addr.Read(senderNode.FlatFindRef(id_address), store);
		}

		void SenderField::Write(MsgWriter& writer) const
		{
			writer.Add("Sender: ");
			m_addr.Write(writer);
			writer.Add("\r\n");
		}


		void ReplyToField::Read(ParseNode const& replyToNode, PinStore& store)
		{
			EnsureThrow(replyToNode.IsType(id_reply_to));
			m_addrList.Read(replyToNode.FlatFindRef(id_address_list), store);
		}

		void ReplyToField::Write(MsgWriter& writer) const
		{
			writer.Add("Reply-To: ");
			m_addrList.Write(writer);
			writer.Add("\r\n");
		}


		void ToField::Read(ParseNode const& toNode, PinStore& store)
		{
			EnsureThrow(toNode.IsType(id_to));
			m_addrList.Read(toNode.FlatFindRef(id_address_list), store);
		}

		void ToField::Write(MsgWriter& writer) const
		{
			writer.Add("To: ");
			m_addrList.Write(writer);
			writer.Add("\r\n");
		}


		void CcField::Read(ParseNode const& ccNode, PinStore& store)
		{
			EnsureThrow(ccNode.IsType(id_cc));
			m_addrList.Read(ccNode.FlatFindRef(id_address_list), store);
		}

		void CcField::Write(MsgWriter& writer) const
		{
			writer.Add("Cc: ");
			m_addrList.Write(writer);
			writer.Add("\r\n");
		}


		void BccField::Read(ParseNode const& bccNode, PinStore& store)
		{
			EnsureThrow(bccNode.IsType(id_bcc));
			ParseNode const* addrListNode = bccNode.FlatFind(id_address_list);
			if (addrListNode)
				m_addrList.Read(*addrListNode, store);
		}

		void BccField::Write(MsgWriter& writer) const
		{
			writer.Add("Bcc: ");
			m_addrList.Write(writer);
			writer.Add("\r\n");
		}


		void MsgIdField::Read(ParseNode const& messageIdNode, PinStore&)
		{
			EnsureThrow(messageIdNode.IsType(id_message_id));

			// Obsolete syntax allows for quoted strings and CFWS within msg-id,
			// but this is troublesome to remove in-depth, and the benefits are unconvincing.
			m_id = messageIdNode.FlatFindRef(Imf::id_msg_id).SrcText();
		}

		void MsgIdField::Write(MsgWriter& writer) const
		{
			writer.Add("Message-ID: ");

			MsgSectionScope { writer };
			writer.Add("<").Add(m_id).Add(">\r\n");
		}


		void SubjectField::Read(ParseNode const& subjectNode, PinStore& store)
		{
			EnsureThrow(subjectNode.IsType(id_subject));
			ParseNode const* unstructuredNode = subjectNode.FlatFind(id_unstructured);
			if (unstructuredNode)
				m_subject = ReadUnstructured(*unstructuredNode, store);
		}

		void SubjectField::Write(MsgWriter& writer) const
		{
			writer.Add("Subject: ");
			WriteUnstructured(writer, m_subject);
			writer.Add("\r\n");
		}


		void AutoSbmtd::Read(ParseNode const& autoSbmtdFieldNode, PinStore& store)
		{
			EnsureThrow(autoSbmtdFieldNode.IsType(id_auto_sbmtd_field));
			for (ParseNode const& c : autoSbmtdFieldNode)
			{
				if (c.IsType(id_auto_sbmtd_value))
				{
					for (ParseNode const& v : c)
					{
						if (v.IsType(id_kw_no            )) { m_value = AutoSbmtdValue::No;                                       break; }
						if (v.IsType(id_kw_auto_gen      )) { m_value = AutoSbmtdValue::AutoGenerated;                            break; }
						if (v.IsType(id_kw_auto_repl     )) { m_value = AutoSbmtdValue::AutoReplied;                              break; }
						if (v.IsType(Mime::id_token_text )) { m_value = AutoSbmtdValue::Extension;     m_extension = v.SrcText(); break; }
					}
				}
				else if (c.IsType(Mime::id_parameter))
					ReadParam(c, store);
			}
		}

		void AutoSbmtd::Write(MsgWriter& writer) const
		{
			writer.Add("Auto-Submitted: ");

			{
				MsgSectionScope section { writer };

				switch (m_value)
				{
				case AutoSbmtdValue::No:            writer.Add("no"); break;
				case AutoSbmtdValue::AutoGenerated: writer.Add("auto-generated"); break;
				case AutoSbmtdValue::AutoReplied:   writer.Add("auto-replied"); break;
				case AutoSbmtdValue::Extension:     writer.Add(m_extension); break;
				default: EnsureThrow(!"Invalid AutoSbmtdValue");
				}

				if (m_params.Any())
					writer.Add("; ");
			}

			WriteParams(writer);
			writer.Add("\r\n");
		}



		// Message

		void Message::ReadHeaders(ParseNode const& messageFieldsNode, PinStore& store)
		{
			EnsureThrow(messageFieldsNode.IsType(id_message_fields));
			ParseNode const& fieldsNode { messageFieldsNode.FlatFindRef(id_main_fields) };
			for (ParseNode const& c : fieldsNode)
			{
					 if (c.IsType(id_orig_date))        m_date     .ReInit().Read(c, store);
				else if (c.IsType(id_from))             m_from     .ReInit().Read(c, store);
				else if (c.IsType(id_sender))           m_sender   .ReInit().Read(c, store);
				else if (c.IsType(id_reply_to))         m_replyTo  .ReInit().Read(c, store);
				else if (c.IsType(id_to))               m_to       .ReInit().Read(c, store);
				else if (c.IsType(id_cc))               m_cc       .ReInit().Read(c, store);
				else if (c.IsType(id_bcc))              m_bcc      .ReInit().Read(c, store);
				else if (c.IsType(id_message_id))       m_msgId    .ReInit().Read(c, store);
				else if (c.IsType(id_subject))          m_subject  .ReInit().Read(c, store);
				else if (c.IsType(id_auto_sbmtd_field)) m_autoSbmtd.ReInit().Read(c, store);
				else
					TryReadMimeField(c, store);
			}
		}

		void Message::WriteHeaders(MsgWriter& writer) const
		{
			if (m_date     .Any()) m_date     ->Write(writer);
			if (m_from     .Any()) m_from     ->Write(writer);
			if (m_sender   .Any()) m_sender   ->Write(writer);
			if (m_replyTo  .Any()) m_replyTo  ->Write(writer);
			if (m_to       .Any()) m_to       ->Write(writer);
			if (m_cc       .Any()) m_cc       ->Write(writer);
			if (m_bcc      .Any()) m_bcc      ->Write(writer);
			if (m_msgId    .Any()) m_msgId    ->Write(writer);
			if (m_subject  .Any()) m_subject  ->Write(writer);
			if (m_autoSbmtd.Any()) m_autoSbmtd->Write(writer);
			WriteMimeFields(writer);
		}


		bool Message::Read(Seq message, PinStore& store)
		{
			m_hdrs = Seq();

			ParseTree pt { message };
			if (!pt.Parse(C_message))
				return false;

			Read(pt, store);
			return true;
		}

		void Message::Read(ParseTree const& ptMessage, PinStore& store)
		{
			m_srcText = ptMessage.Root().SrcText();

			ParseNode const& messageFieldsNode { ptMessage.Root().FrontFindRef(id_message_fields) };
			m_hdrs = messageFieldsNode.SrcText();
			ReadHeaders(messageFieldsNode, store);

			ParseNode const* bodyNode { ptMessage.Root().FirstChild().FlatFind(id_body) };
			if (bodyNode != nullptr)
				m_contentEncoded = bodyNode->SrcText();

			LocateParts();
		}

		bool Message::ReadMultipartBody(PinStore& store, Mime::PartReadCx& prcx)
		{
			if (!IsMultipart()) return false;
			bool success = Part::ReadMultipartBody(m_contentEncoded, store, prcx);
			LocateParts();
			return success;
		}

		void Message::Write(MsgWriter& writer) const
		{
			m_hdrs = Seq();
			sizet hdrsStartPos = writer.GetStr().Len();
			WriteHeaders(writer);
			sizet hdrsEndPos = writer.GetStr().Len();

			writer.Add("\r\n");
			
			if (!IsMultipart() || !m_parts.Any())
			{
				writer.AddVerbatim(m_contentEncoded);
				if (!m_contentEncoded.EndsWithExact("\r\n"))
					writer.AddVerbatim("\r\n");
			}
			else
			{
				// If not set previously by caller, boundary is set in WriteMimeFields()
				Seq boundary { m_contentType->m_params.Get("boundary") };
				EnsureThrow(boundary.n);
				WriteMultipartBody(writer, boundary);
			}

			EnsureThrow(hdrsEndPos <= writer.GetStr().Len());
			EnsureThrow(hdrsStartPos <= hdrsEndPos);
			m_hdrs = Seq { writer.GetStr().Ptr() + hdrsStartPos, hdrsEndPos - hdrsStartPos };
		}


		void Message::LocateParts()
		{
			// Search plan for plain text message content:
			//   multipart/mixed       -> 1st child == multipart/alternative -> 1st child == text/plain
			//   multipart/mixed       -> any child == text/plain
			//   multipart/alternative -> 1st child == text/plain
			//   text/plain
			//   message content without Content-Type
			//
			// Search plan for message/delivery-status:
			//   multipart/report      -> any child == message/delivery-status
			//   multipart/mixed       -> any child == message/delivery-status
			//
			// Search plan for HTML message content:
			//   multipart/mixed       -> 1st child == multipart/alternative -> any child == multipart/related -> 1st child == text/html
			//   multipart/mixed       -> 1st child == multipart/alternative -> any child == text/html
			//   multipart/mixed       -> 1st child == multipart/related     -> 1st child == text/html
			//   multipart/mixed       -> any child == text/html
			//   multipart/alternative -> any child == multipart/related     -> 1st child == text/html
			//   multipart/alternative -> any child == text/html
			//   multipart/related     -> 1st child == text/html
			//   text/html
			//
			// Search plan for inline content referenced by HTML:
			//   multipart/mixed       -> 1st  child == multipart/alternative -> any  child == multipart/related -> 2nd+ child
			//   multipart/mixed       -> 1st  child == multipart/related     -> 2nd+ child
			//   multipart/alternative -> any  child == multipart/related     -> 2nd+ child
			//   multipart/related     -> 2nd+ child
			//   
			// Search plan for attachments:
			//   multipart/mixed       -> 2nd+ child
			// 
			// Remarks:
			// - multipart/report is treated as an alias for multipart/mixed
			// - if multipart/mixed or multipart/report contains a first child that's also multipart/mixed or multipart/report,
			//   the outer is treated as non-substantial and the inner one is searched instead

			if (!m_contentType.Any())
				m_plainTextContentPart = this;
			else if (!m_contentType->IsMultipart())
			{
				if (m_contentType->IsTextPlain())
					m_plainTextContentPart = this;
				else if (m_contentType->IsTextHtml())
					m_htmlContentPart = this;
			}
			else if (m_parts.Any())
			{
				if (m_contentType->IsMultipartMixed() || m_contentType->IsMultipartReport())
					LocateParts_Mixed(*this);
				else if (m_contentType->IsMultipartAlternative())
					LocateParts_Alternative(*this);
				else if (m_contentType->IsMultipartRelated())
					LocateParts_Related(*this);
			}
		}


		void Message::LocateParts_Mixed(Mime::Part const& mixPart)
		{
			EnsureThrow(!m_plainTextContentPart);
			EnsureThrow(!m_htmlContentPart);
			EnsureThrow(!m_deliveryStatusPart);

			sizet firstAttachmentPart = 1;

			{
				Mime::Part const& firstPart = mixPart.m_parts.First();					
				if (!firstPart.m_contentType.Any())
					m_plainTextContentPart = &firstPart;
				else
				{
					if (firstPart.m_contentType->IsMultipartMixed() || firstPart.m_contentType->IsMultipartReport())
						LocateParts_Mixed(firstPart);
					else if (firstPart.m_contentType->IsMultipartAlternative())
						LocateParts_Alternative(firstPart);
					else if (firstPart.m_contentType->IsMultipartRelated())
						LocateParts_Related(firstPart);
					else
						firstAttachmentPart = 0;
				}
			}

			// Attachments
			for (Mime::Part const& part : mixPart.m_parts.GetSlice(firstAttachmentPart))
			{
				bool special {};
				if (part.m_contentType.Any())
				{
					     if (!m_plainTextContentPart && part.m_contentType->IsTextPlain())         { m_plainTextContentPart = &part; special = true; }
					else if (!m_htmlContentPart      && part.m_contentType->IsTextHtml())          { m_htmlContentPart      = &part; special = true; }
					else if (!m_deliveryStatusPart   && part.m_contentType->IsMsgDeliveryStatus()) { m_deliveryStatusPart   = &part; special = true; }
				}

				if (!special)
					m_attachmentParts.Add(&part);
			}
		}

		
		void Message::LocateParts_Alternative(Mime::Part const& altPart)
		{
			EnsureThrow(!m_plainTextContentPart);
			EnsureThrow(!m_htmlContentPart);

			for (Mime::Part const& part : altPart.m_parts)
			{
				if (part.m_contentType.Any())
				{
					if (part.m_contentType->IsTextPlain())
					{
						if (!m_plainTextContentPart)
							m_plainTextContentPart = &part;
					}
					else if (part.m_contentType->IsTextHtml())
					{
						if (!m_htmlContentPart)
							m_htmlContentPart = &part;
					}
					else if (part.m_contentType->IsMultipartRelated())
					{
						if (!m_htmlContentPart)
							LocateParts_Related(part);
					}
				}

				if (m_plainTextContentPart != nullptr && m_htmlContentPart != nullptr)
					break;
			}
		}

		
		void Message::LocateParts_Related(Mime::Part const& relPart)
		{
			EnsureThrow(!m_htmlContentPart);

			bool isFirst = true;
			for (Mime::Part const& part : relPart.m_parts)
			{
				if (isFirst)
				{
					if (!part.m_contentType.Any() || !part.m_contentType->IsTextHtml())
						break;

					m_htmlContentPart = &part;
					isFirst = false;
				}
				else
				{
					m_htmlRelatedParts.Add(&part);
				}
			}
		}



		// Email addresses

		bool ReadEmailAddressAsAddrSpec(Seq address, AddrSpecWithStore& addrSpec)
		{
			ParseTree pt { address };
			if (!pt.Parse(C_addr_spec))
				return false;

			ParseNode const& addrSpecNode { pt.Root().FlatFindRef(id_addr_spec) };
			addrSpec.m_pinStore.Init(address.n);
			addrSpec.Read(addrSpecNode, addrSpec.m_pinStore.Ref());
			return true;
		}


		bool ExtractLocalPartAndDomainFromEmailAddress(Seq address, Str* localPart, Str* domain)
		{
			AddrSpecWithStore addrSpec;
			if (!ReadEmailAddressAsAddrSpec(address, addrSpec))
				return false;

			if (localPart)
				localPart->Set(addrSpec.m_localPart);
			if (domain)
				domain->Set(addrSpec.m_domain);
	
			return true;
		}



		void NormalizeEmailAddress(ParseTree const& ptAddrSpec, PinStore& store, Str& normalized)
		{
			ParseNode const& addrSpecNode { ptAddrSpec.Root().FlatFindRef(id_addr_spec) };
			AddrSpec addrSpec;
			addrSpec.Read(addrSpecNode, store);
			normalized.Clear().Obj(addrSpec);
		}


		bool NormalizeEmailAddress(Seq address, Str& normalized)
		{
			ParseTree pt { address };
			if (!pt.Parse(C_addr_spec))
				return false;

			PinStore store { address.n };
			NormalizeEmailAddress(pt, store, normalized);
			return true;
		}


		bool NormalizeEmailAddressInPlace(Str& address)
		{
			Str normalized;
			if (!NormalizeEmailAddress(address, normalized))
				return false;

			address = std::move(normalized);
			return true;
		}


		namespace
		{
			bool ForEachAddressInParseNodeAndChildren(PinStore& store, sizet& nrEnumerated, ParseNode const& pn, std::function<bool(Str&& addr)> f)
			{
				if (pn.IsType(Imf::id_mailbox))
				{
					++nrEnumerated;

					Mailbox mbox;
					mbox.Read(pn, store);

					Str addr;
					addr.ReserveExact(pn.SrcText().n);

					MsgWriter writer { addr };
					mbox.Write(writer);

					if (!f(std::move(addr)))
						return false;
				}
				else if (pn.IsType(Imf::id_addr_spec))
				{
					++nrEnumerated;

					AddrSpec addrSpec;
					addrSpec.Read(pn, store);

					Str addr;
					addr.ReserveExact(pn.SrcText().n);

					MsgWriter writer { addr };
					addrSpec.Write(writer);

					if (!f(std::move(addr)))
						return false;
				}
				else if (pn.HaveChildren())
				{
					for (ParseNode const& c : pn)
						if (!ForEachAddressInParseNodeAndChildren(store, nrEnumerated, c, f))
							return false;
				}

				return true;
			}
		}	// anon


		ForEachAddressResult ForEachAddressInCasualEmailAddressList(Seq casualAddressList, std::function<bool(Str&& addr)> f)
		{
			ForEachAddressResult r;

			casualAddressList = casualAddressList.Trim();
			if (!casualAddressList.n)
				return r;

			ParseTree pt { casualAddressList };
			if (!pt.Parse(Imf::C_casual_addr_list))
			{
				r.m_parseError.Set("Could not parse email address list. ").Obj(pt, ParseTree::BestAttempt);
				return r;
			}

			PinStore store { casualAddressList.n };
			ForEachAddressInParseNodeAndChildren(store, r.m_nrEnumerated, pt.Root(), f);
			return r;
		}



		// Domains

		bool IsValidDomain(Seq domain)
		{
			return ParseTree(domain).Parse(Imf::C_domain);
		}


		bool IsEqualOrSubDomainOf(Seq exemplar, Seq other)
		{
			if (other.EqualInsensitive(exemplar))
				return true;

			if (other.n > exemplar.n && other.EndsWithInsensitive(exemplar))
				if (other.p[other.n - exemplar.n - 1] == '.')
					if (IsValidDomain(other))
						return true;

			return false;
		}

	}  // namespace Imf
}
