#include "AtIncludes.h"
#include "AtSmtpReceiverThread.h"

#include "AtNumCvt.h"
#include "AtSmtpGrammar.h"
#include "AtSocketReader.h"
#include "AtSocketWriter.h"


namespace At
{

	namespace
	{

		// SmtpReceiver_Disconnect

		struct SmtpReceiver_Disconnect : public StrErr
		{
		public:
			uint const m_code;

			SmtpReceiver_Disconnect(uint code, Seq msg) : m_code(code), StrErr(msg) {};
		};



		// SmtpReceiver_ClientMsgData

		class SmtpReceiver_ClientMsgData
		{
		public:
			Str m_data;
	
			void ReadMsgData(Reader& reader, sizet approxMaxDataBytes);
		};


		void SmtpReceiver_ClientMsgData::ReadMsgData(Reader& reader, sizet approxMaxDataBytes)
		{
			bool receivedLastLine = false;
			do
			{
				reader.SetExpireMs(EmailServer_RecvTimeoutMs);
				reader.Read( [&] (Seq& avail) -> Reader::Instr::E
					{
						Seq const line { avail.ReadToString("\r\n") };
						if (m_data.Len() + line.n > approxMaxDataBytes)
							throw SmtpReceiver_Disconnect(554, "Message too large");

						if (avail.n)
						{
							if (line == ".")
								receivedLastLine = true;
							else
							{
								Seq toAppend = line;
								if (toAppend.StartsWithExact("."))
									toAppend.DropByte();

								m_data.Add(toAppend).Add("\r\n");
							}

							avail.DropBytes(2);
							return Reader::Instr::Done;
						}

						return Reader::Instr::NeedMore;
					} );
			}
			while (!receivedLastLine);
		}

	} // anonymous namespace



	// SmtpReceiverThread

	void SmtpReceiverThread::WorkPoolThread_ProcessWorkItem(void* pvWorkItem)
	{
		AutoFree<EmailServerWorkItem> workItem = (EmailServerWorkItem*) pvWorkItem;

		SmtpReceiverCfg cfg { Entity::Contained };
		m_workPool->SmtpReceiver_GetCfg(cfg);

		EmailSrvBinding const* binding = EmailSrvBindings_FindPtrByToken(cfg.f_bindings, workItem->m_bindingToken);

		Str ourName;
		if (binding && binding->f_computerName.Any()) ourName = binding->f_computerName;
		else                                          ourName = cfg.f_computerName;

		uint64 maxInMsgBytes;
		if (binding && binding->f_maxInMsgKb != 0) maxInMsgBytes = binding->f_maxInMsgKb * 1024;
		else                                       maxInMsgBytes = cfg.f_maxInMsgKb * 1024;

		SockInit sockInit;
		Socket& sk = workItem->m_sk;

		// It is NOT a good idea to enable TCP_NODELAY here.
		// It would cause inefficiency in response sending in combination with PIPELINING.

		SocketReader reader { sk.GetSocket() };
		SocketWriter writer { sk.GetSocket() };
		Schannel conn { &reader, &writer };
		conn.SetStopCtl(*this);

		try
		{
			try
			{
				Rp<SmtpReceiverAuthCx> l_authCx;
				EhloHost l_ehloHost;
				bool     l_haveMailFrom {};
				uint64   l_fromSize     {};
				sizet    l_maxDataBytes {};
				Vec<Str> l_toMailboxes;

				auto rset = [&] ()
					{
						l_haveMailFrom = false;
						l_fromSize = 0;
						l_maxDataBytes = 0;
						l_toMailboxes.Clear();
					};

				auto startTls = [&] ()
					{
						m_workPool->SmtpReceiver_AddSchannelCerts(conn);
						conn.InitCred(ProtoSide::Server);
						conn.SetExpireMs(EmailServer_RecvTimeoutMs);
						conn.StartTls();
					};

				if (!binding)
					throw SmtpReceiver_Disconnect(421, "Cannot accept connection: configuration changed, binding no longer present");

				if (binding->f_implicitTls)
					startTls();

				SendReply(conn, 220, Str::Join(ourName, " ESMTP Ready"));

				while (true)
				{
					EmailServer_ClientCmd cmd;
					cmd.ReadCmd(conn);
				
					if (cmd.m_cmd == "quit")
					{
						SendReply(conn, 221, "QUIT command successful");
						break;
					}
					
					if (cmd.m_cmd == "helo")
					{
						l_ehloHost = ParseEhloHost(cmd.m_params);
						SendReply(conn, 250, ourName);
					}
					else if (cmd.m_cmd == "ehlo")
					{
						l_ehloHost = ParseEhloHost(cmd.m_params);

						// Supported: SIZE (RFC 1870); PIPELINING (RFC 2920); STARTTLS (RFC 3207); 8BITMIME (RFC 6152)
						Str ehloReply;
						ehloReply.SetAdd(ourName,
							"\r\nPIPELINING"
							"\r\n8BITMIME"
							"\r\nSIZE ").UInt(maxInMsgBytes);

						if (!conn.TlsStarted() && m_workPool->SmtpReceiver_TlsSupported())
							ehloReply.Add("\r\nSTARTTLS");

						if (conn.TlsStarted() && m_workPool->SmtpReceiver_AuthSupported())
						{
							// We require TLS before AUTH so that we can use AUTH PLAIN
							// We prefer AUTH PLAIN so that passwords can be stored hashed
							// Authentication types like CRAM-MD5 require storing passwords in plaintext
							ehloReply.Add("\r\nAUTH PLAIN");
						}

						SendReply(conn, 250, ehloReply);
						rset();
					}
					else if (cmd.m_cmd == "starttls" && m_workPool->SmtpReceiver_TlsSupported())
					{
						if (cmd.m_params.Any())
							SendReply(conn, 501, "STARTTLS command failed, no parameters allowed");
						else if (conn.TlsStarted())
							SendReply(conn, 503, "STARTTLS command unavailable, TLS already started");
						else
						{
							SendReply(conn, 220, "STARTTLS command successful, ready to start TLS");
							rset();
							startTls();
						}
					}
					else if (cmd.m_cmd == "auth" && conn.TlsStarted() && m_workPool->SmtpReceiver_AuthSupported())
					{
						if (l_authCx.Any())
							SendReply(conn, 503, "AUTH command unavailable, already authenticated");
						else
						{
							Seq paramsReader = cmd.m_params;
							SeqLower const authType { paramsReader.ReadToFirstByteOfType(Ascii::IsWhitespace) };
						
							paramsReader.DropToFirstByteNotOfType(Ascii::IsWhitespace);
						
							if (authType != "plain")
								SendReply(conn, 504, "AUTH command failed, unsupported authentication type");
							else
							{
								EmailServer_ClientLine authDataLine;
								Str authDataBin;

								if (!paramsReader.n)
								{
									SendReply(conn, 334, "");
									authDataLine.ReadLine(conn);
									paramsReader = authDataLine.m_line;
								}

								Base64::MimeDecode(paramsReader, authDataBin);
								if (paramsReader.n)
									SendReply(conn, 501, "Not all AUTH PLAIN authentication data could be base64-decoded");
								else if (!authDataBin.Any())
									SendReply(conn, 501, "AUTH PLAIN authentication data is empty");
								else
								{
									Seq authDataReader = authDataBin;
									Seq const authorizationIdentity = authDataReader.ReadToByte(0).Trim();
									if (!authDataReader.n)
										SendReply(conn, 501, "AUTH PLAIN authentication data is missing first null separator");
									else
									{
										authDataReader.DropByte();
										Seq const authenticationIdentity = authDataReader.ReadToByte(0).Trim();
										if (!authDataReader.n)
											SendReply(conn, 501, "AUTH PLAIN authentication data is missing second null separator");
										else
										{
											authDataReader.DropByte();
											Seq const password = authDataReader.Trim();

											EmailServerAuthResult result = m_workPool->SmtpReceiver_Authenticate(workItem->m_saRemote, conn, ourName, l_ehloHost,
												authorizationIdentity, authenticationIdentity, password, l_authCx);

											if (EmailServerAuthResult::Success == result)
											{
												EnsureThrow(l_authCx.Any());
												SendReply(conn, 235, "AUTH command successful, user authenticated");
											}
											else
											{
												l_authCx.Clear();

												switch (result)
												{
												case EmailServerAuthResult::InvalidCredentials:  SendReply(conn, 535, "5.7.8 AUTH command failed, credentials invalid"); break;
												case EmailServerAuthResult::AttemptsTooFrequent: SendReply(conn, 454, "4.7.0 AUTH command failed, attempts too frequent"); break;
												case EmailServerAuthResult::TransactionFailed:   SendReply(conn, 454, "4.7.0 AUTH command failed, could not complete transaction"); break;
												default: EnsureThrowWithCode(!"Unexpected EmailServerAuthResult", (int64) result);
												}
											}
										}
									}
								}
							}
						}
					}
					else if (cmd.m_cmd == "rset")
					{
						SendReply(conn, 250, "RSET command successful");
						rset();
					}
					else if (cmd.m_cmd == "noop")
					{
						SendReply(conn, 250, "NOOP command successful");
					}
					else if (cmd.m_cmd == "vrfy")
					{
						SendReply(conn, 252, "VRFY command is a no-op");
					}
					else if (cmd.m_cmd == "mail")
					{
						Seq params { cmd.m_params };
						if (l_haveMailFrom)
							SendReply(conn, 503, "MAIL command already processed");
						else if (!params.StripPrefixInsensitive("from:"))
							SendReply(conn, 502, "Unrecognized form of MAIL command");
						else
						{
							// Parse fromMailbox from params
							ParseTree parseTree { params };
							parseTree.RecordBestToStack();
							if (!parseTree.Parse(Smtp::C_MailParams))
								SendReply(conn, 502, Str("Error parsing MAIL FROM command parameters\r\n").Obj(parseTree, ParseTree::BestAttempt));
							else
							{
								Seq fromMailbox;
								ParseNode const* mailboxNode = parseTree.Root().DeepFind(Smtp::id_Mailbox);
								if (mailboxNode)
									fromMailbox = mailboxNode->SrcText();
	
								uint64 fromSize {};
								ParseNode const* sizeValNode = parseTree.Root().DeepFind(Smtp::id_SizeParamVal);
								if (sizeValNode)
									fromSize = sizeValNode->SrcText().ReadNrUInt64Dec();

								if (fromSize > maxInMsgBytes)
									SendNegativeReply(conn, 552, "MAIL FROM command failed, message exceeds declared maximum size");
								else
								{
									Opt<SmtpReceiveInstruction> instr;
									if (l_authCx.Any())
										instr.Init(l_authCx->SmtpReceiverAuthCx_OnMailFrom_HaveAuth(fromMailbox));
									else
									{
										instr.Init(m_workPool->SmtpReceiver_OnMailFrom_NoAuth(workItem->m_saRemote, conn, ourName, l_ehloHost, fromMailbox, l_authCx));
										if (instr->m_accept)
											EnsureThrow(l_authCx.Any());
									}

									if (!instr->m_accept)
										SendNegativeReply(conn, instr->m_replyCode, instr->m_replyText);
									else if (instr->m_dataBytesToAccept < fromSize)
										SendNegativeReply(conn, 552, "MAIL FROM command failed, cannot accept a message of this size from this sender");
									else
									{
										SendReply(conn, 250, "MAIL FROM command successful");

										l_haveMailFrom = true;
										l_fromSize     = fromSize;
										l_maxDataBytes = instr->m_dataBytesToAccept;
									}
								}
							}
						}
					}
					else if (cmd.m_cmd == "rcpt")
					{
						Seq params { cmd.m_params };
						if (!params.StripPrefixInsensitive("to:"))
							SendReply(conn, 502, "Unrecognized form of RCPT command");
						else if (!l_haveMailFrom)
							SendReply(conn, 503, "RCPT TO command must be preceded by a successful MAIL FROM");
						else
						{
							EnsureThrow(l_authCx.Any());

							Seq toMailbox;
						
							// Parse toMailbox from params
							ParseTree parseTree(params);
							if (!parseTree.Parse(Smtp::C_Path))
								SendReply(conn, 502, "RCPT TO command failed, forward-path could not be parsed");
							else
							{
								ParseNode const* parserMailbox = parseTree.Root().DeepFind(Smtp::id_Mailbox);
								if (!parserMailbox)
									SendReply(conn, 502, "RCPT TO command failed, forward-path does not contain a Mailbox");
								else
								{
									toMailbox = parserMailbox->SrcText();

									SmtpReceiveInstruction instr = l_authCx->SmtpReceiverAuthCx_OnRcptTo(toMailbox);
									if (!instr.m_accept)
										SendNegativeReply(conn, instr.m_replyCode, instr.m_replyText);
									else if (instr.m_dataBytesToAccept < l_fromSize)
										SendNegativeReply(conn, 552, "RCPT TO command failed, cannot accept a message of this size for this recipient");
									else
									{
										SendReply(conn, 250, "RCPT TO command successful");
									
										if (l_maxDataBytes < instr.m_dataBytesToAccept)
											l_maxDataBytes = instr.m_dataBytesToAccept;
										l_toMailboxes.Add(toMailbox);
									}
								}
							}
						}
					}
					else if (cmd.m_cmd == "data")
					{
						if (!l_haveMailFrom)
							SendReply(conn, 503, "DATA command must be preceded by a successful MAIL FROM");
						else if (!l_toMailboxes.Any())
							SendReply(conn, 503, "DATA command must be preceded by a successful RCPT TO");
						else
						{
							EnsureThrow(l_authCx.Any());

							SendReply(conn, 354, "Ready to receive message data");
						
							SmtpReceiver_ClientMsgData data;
							data.ReadMsgData(conn, l_maxDataBytes);
						
							SmtpReceiveInstruction instr = l_authCx->SmtpReceiverAuthCx_OnData(l_toMailboxes, data.m_data);
							if (!instr.m_accept)
								SendNegativeReply(conn, instr.m_replyCode, instr.m_replyText);
							else
								SendReply(conn, 250, "DATA command successful, message accepted");
						
							rset();
						}
					}
					else
						SendReply(conn, 500, "Unrecognized SMTP command");
				}
			}
			catch (EmailServer_Disconnect const& e)
				{ SendReply(conn, 500, e.what()); }
			catch (SmtpReceiver_Disconnect const& e)
				{ SendReply(conn, e.m_code, e.what()); }
		}
		catch (CommunicationErr const&)
		{
			// Just close the connection.
		}
	}


	EhloHost SmtpReceiverThread::ParseEhloHost(Seq host)
	{
		host = host.Trim();
		if (!host.Any())
			return EhloHost(EhloHostType::Empty, Seq());

		ParseTree pt { host };
		if (!pt.Parse(Smtp::C_Domain_or_AddrLit))
			return EhloHost(EhloHostType::Invalid, host);

		if (pt.Root().FlatFind(Smtp::id_Domain))
			return EhloHost(EhloHostType::Domain, host);

		if (pt.Root().FlatFind(Smtp::id_AddrLit))
			return EhloHost(EhloHostType::AddrLit, host);

		return EhloHost(EhloHostType::Unexpected, host);
	}


	void SmtpReceiverThread::SendReply(Writer& writer, uint code, Seq text)
	{
		EnsureThrow(code >= 100 && code <= 999);

		Str linesToSend;
		linesToSend.ReserveExact(2 * (4 + text.n + 2));

		while (true)
		{
			Seq line = text.ReadToString("\r\n");
			text.DropBytes(2);
			if (text.n)
				linesToSend.UInt(code).Ch('-').Add(line).Add("\r\n");
			else
			{
				linesToSend.UInt(code).Ch(' ').Add(line).Add("\r\n");
				break;
			}
		}
	
		writer.SetExpireMs(EmailServer_SendTimeoutMs);
		writer.Write(linesToSend);
	}


	void SmtpReceiverThread::SendNegativeReply(Writer& writer, uint code, Seq text)
	{
		EnsureThrow(code >= 400 && code <= 599);
		SendReply(writer, code, text);
	}

}
