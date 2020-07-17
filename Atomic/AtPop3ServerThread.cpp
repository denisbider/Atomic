#include "AtIncludes.h"
#include "AtPop3ServerThread.h"

#include "AtSocketReader.h"
#include "AtSocketWriter.h"


namespace At
{

	void Pop3ServerThread::WorkPoolThread_ProcessWorkItem(void* pvWorkItem)
	{
		SockInit sockInit;
		AutoFree<EmailServerWorkItem> workItem = (EmailServerWorkItem*) pvWorkItem;
		Socket& sk = workItem->m_sk;

		Pop3ServerCfg cfg { Entity::Contained };
		m_workPool->Pop3Server_GetCfg(cfg);

		EmailSrvBinding const* binding = EmailSrvBindings_FindPtrByToken(cfg.f_bindings, workItem->m_bindingToken);

		Str ourName;
		if (binding && binding->f_computerName.Any()) ourName = binding->f_computerName;
		else                                          ourName = cfg.f_computerName;

		SocketReader reader { sk.GetSocket() };
		SocketWriter writer { sk.GetSocket() };
		Schannel conn { &reader, &writer };
		conn.SetStopCtl(*this);

		try
		{
			try
			{
				Rp<Pop3ServerAuthCx> l_authCx;
				Str l_userName;
				Str l_password;
				Vec<Pop3ServerMsgEntry> l_msgs;

				auto startTls = [&] ()
					{
						m_workPool->Pop3Server_AddSchannelCerts(ourName, conn);
						conn.InitCred(ProtoSide::Server);
						conn.SetExpireMs(EmailServer_RecvTimeoutMs);
						conn.StartTls();
					};

				auto checkHaveAuth = [&] () -> bool
					{
						if (!conn.TlsStarted()) { SendSingleLineReply(conn, Pop3ReplyType::Err, "Command unavailable, must start TLS and authenticate"); return false; }
						if (!l_authCx.Any())    { SendSingleLineReply(conn, Pop3ReplyType::Err, "Command unavailable, must authenticate"); return false; }
						return true;
					};

				auto checkGetMsgIndex = [&] (Seq reader, sizet& msgIndex) -> bool
					{
						reader = reader.Trim();
						uint64 msgNr = reader.ReadNrUInt64Dec();
						if (reader.n)                          { SendSingleLineReply(conn, Pop3ReplyType::Err, "Command failed, unrecognized syntax"); return false; }
						if (msgNr < 1 || msgNr > l_msgs.Len()) { SendSingleLineReply(conn, Pop3ReplyType::Err, "Command failed, no such message"); return false; }
						msgIndex = (sizet) (msgNr - 1);
						if (!l_msgs[msgIndex].m_present)       { SendSingleLineReply(conn, Pop3ReplyType::Err, "Command failed, message has been deleted"); return false; }
						return true;
					};

				auto getMsgStats = [&] (uint64& nrMsgs, uint64& totalBytes)
					{
						nrMsgs = 0;
						totalBytes = 0;

						for (Pop3ServerMsgEntry const& msg : l_msgs)
							if (msg.m_present)
							{
								++nrMsgs;
								totalBytes += msg.m_nrBytes;
							}
					};

				if (!binding)
					throw EmailServer_Disconnect("Cannot accept connection: configuration changed, binding no longer present");

				if (binding->f_implicitTls)
					startTls();
				
				SendSingleLineReply(conn, Pop3ReplyType::OK, "POP3 Ready");

				while (true)
				{
					EmailServer_ClientCmd cmd;
					cmd.ReadCmd(conn);
				
					if (cmd.m_cmd == "quit")
					{
						bool haveErr {};
						if (l_authCx.Any())
							if (!l_authCx->Pop3ServerAuthCx_Commit(l_msgs))
							{
								haveErr = true;
								SendSingleLineReply(conn, Pop3ReplyType::Err, "QUIT command failed, error committing message state");
							}

						if (!haveErr)
							SendSingleLineReply(conn, Pop3ReplyType::OK, "QUIT command successful");
						break;
					}
					else if (cmd.m_cmd == "capa")
					{
						Str capaReply = "CAPA command successful, capabilities follow\r\n"
							"UIDL\r\n"
							"RESP-CODES\r\n"
							"AUTH-RESP-CODE\r\n"
							"PIPELINING\r\n";

						if (!conn.TlsStarted())
							capaReply.Add("STLS\r\n");
						else
							capaReply.Add("USER\r\n");

						SendMultiLineReply(conn, capaReply);
					}
					else if (cmd.m_cmd == "noop")
					{
						SendSingleLineReply(conn, Pop3ReplyType::OK, "NOOP command successful");
					}
					else if (cmd.m_cmd == "stls")
					{
						if (conn.TlsStarted())
							SendSingleLineReply(conn, Pop3ReplyType::Err, "STLS command unavailable, TLS already started");
						else
						{
							SendSingleLineReply(conn, Pop3ReplyType::OK, "STLS command successful, ready to start TLS");
							startTls();
						}
					}
					else if (cmd.m_cmd == "user")
					{
						if (!conn.TlsStarted())  SendSingleLineReply(conn, Pop3ReplyType::Err, "USER command failed, requires TLS");
						else if (l_authCx.Any()) SendSingleLineReply(conn, Pop3ReplyType::Err, "USER command unavailable, already authenticated");
						else
						{
							l_userName = Seq(cmd.m_params).Trim();
							if (!l_userName.Any()) SendSingleLineReply(conn, Pop3ReplyType::Err, "USER command failed, user name cannot be empty");
							else                   SendSingleLineReply(conn, Pop3ReplyType::OK, "USER command successful, expecting password");
						}
					}
					else if (cmd.m_cmd == "pass")
					{
						     if (!conn.TlsStarted()) SendSingleLineReply(conn, Pop3ReplyType::Err, "PASS command failed, requires TLS");
						else if (l_authCx.Any())     SendSingleLineReply(conn, Pop3ReplyType::Err, "PASS command unavailable, already authenticated");
						else if (!l_userName.Any())  SendSingleLineReply(conn, Pop3ReplyType::Err, "PASS command failed, must be preceded by USER");
						else
						{
							l_password = Seq(cmd.m_params).Trim();
							EmailServerAuthResult result = m_workPool->Pop3Server_Authenticate(workItem->m_saRemote, conn, l_userName, l_password, l_authCx);

							if (EmailServerAuthResult::Success == result)
							{
								EnsureThrow(l_authCx.Any());
								if (l_authCx->Pop3ServerAuthCx_GetMsgs(l_msgs))
									SendSingleLineReply(conn, Pop3ReplyType::OK, "PASS command successful, user authenticated");
								else
								{
									l_authCx.Clear();
									SendSingleLineReply(conn, Pop3ReplyType::Err, "[SYS/TEMP] PASS command failed, error retrieving messages");
								}
							}
							else
							{
								l_authCx.Clear();

								switch (result)
								{
								case EmailServerAuthResult::InvalidCredentials:  SendSingleLineReply(conn, Pop3ReplyType::Err, "[AUTH] PASS command failed, credentials invalid"); break;
								case EmailServerAuthResult::AttemptsTooFrequent: SendSingleLineReply(conn, Pop3ReplyType::Err, "[AUTH] PASS command failed, attempts too frequent"); break;
								case EmailServerAuthResult::TransactionFailed:   SendSingleLineReply(conn, Pop3ReplyType::Err, "[SYS/TEMP] PASS command failed, could not complete transaction"); break;
								default: EnsureThrowWithNr(!"Unexpected EmailServerAuthResult", (int64) result);
								}
							}
						}
					}
					else if (cmd.m_cmd == "stat")
					{
						if (checkHaveAuth())
						{
							uint64 nrMsgs, totalBytes;
							getMsgStats(nrMsgs, totalBytes);

							Str line;
							line.UInt(nrMsgs).Ch(' ').UInt(totalBytes);
							SendSingleLineReply(conn, Pop3ReplyType::OK, line);
						}
					}
					else if (cmd.m_cmd == "list")
					{
						if (checkHaveAuth())
						{
							Seq params = Seq(cmd.m_params).Trim();
							if (!params.n)
							{
								uint64 nrMsgs, totalBytes;
								getMsgStats(nrMsgs, totalBytes);

								Str lines;
								lines.ReserveExact(NumCast<size_t>(50ULL + (nrMsgs * 30ULL)));
								lines.UInt(nrMsgs).Add(" messages (").UInt(totalBytes).Add(" octets)\r\n");

								for (sizet i=0; i!=l_msgs.Len(); ++i)
									if (l_msgs[i].m_present)
										lines.UInt(i+1).Ch(' ').UInt(l_msgs[i].m_nrBytes).Add("\r\n");

								SendMultiLineReply(conn, lines);
							}
							else
							{
								sizet msgIndex;
								if (checkGetMsgIndex(params, msgIndex))
								{
									Str line;
									line.UInt(msgIndex+1).Ch(' ').UInt(l_msgs[msgIndex].m_nrBytes);
									SendSingleLineReply(conn, Pop3ReplyType::OK, line);
								}
							}
						}
					}
					else if (cmd.m_cmd == "uidl")
					{
						if (checkHaveAuth())
						{
							Seq params = Seq(cmd.m_params).Trim();
							if (!params.n)
							{
								uint64 nrMsgs, totalBytes;
								getMsgStats(nrMsgs, totalBytes);

								Str lines;
								lines.ReserveExact(NumCast<size_t>(50ULL + (nrMsgs * 75ULL)));
								lines.UInt(nrMsgs).Add(" messages (").UInt(totalBytes).Add(" octets)\r\n");

								for (sizet i=0; i!=l_msgs.Len(); ++i)
									if (l_msgs[i].m_present)
										lines.UInt(i+1).Ch(' ').Add(l_msgs[i].m_uniqueId).Add("\r\n");

								SendMultiLineReply(conn, lines);
							}
							else
							{
								sizet msgIndex;
								if (checkGetMsgIndex(params, msgIndex))
								{
									Str line;
									line.UInt(msgIndex+1).Ch(' ').Add(l_msgs[msgIndex].m_uniqueId);
									SendSingleLineReply(conn, Pop3ReplyType::OK, line);
								}
							}
						}
					}
					else if (cmd.m_cmd == "retr")
					{
						if (checkHaveAuth())
						{
							sizet msgIndex;
							if (checkGetMsgIndex(cmd.m_params, msgIndex))
							{
								sizet nrBytes = l_msgs[msgIndex].m_nrBytes;

								Str lines;
								lines.ReserveExact(30 + nrBytes);
								lines.UInt(nrBytes).Add(" octets\r\n");
								if (!l_authCx->Pop3ServerAuthCx_GetMsgContent(msgIndex, lines))
									SendSingleLineReply(conn, Pop3ReplyType::Err, "RETR command failed, message content could not be retrieved");
								else
									SendMultiLineReply(conn, lines);
							}
						}
					}
					else if (cmd.m_cmd == "dele")
					{
						if (checkHaveAuth())
						{
							sizet msgIndex;
							if (checkGetMsgIndex(cmd.m_params, msgIndex))
							{
								l_msgs[msgIndex].m_present = false;

								Str line;
								line.Set("DELE command successful, message ").UInt(msgIndex+1).Add(" marked for deletion");
								SendSingleLineReply(conn, Pop3ReplyType::OK, line);
							}
						}
					}
					else if (cmd.m_cmd == "rset")
					{
						if (checkHaveAuth())
						{
							for (Pop3ServerMsgEntry& msg : l_msgs)
								msg.m_present = true;

							uint64 nrMsgs, totalBytes;
							getMsgStats(nrMsgs, totalBytes);

							Str line;
							line.Set("RSET command successful, ").UInt(nrMsgs).Add(" messages (").UInt(totalBytes).Add(" octets)");
							SendSingleLineReply(conn, Pop3ReplyType::OK, line);
						}
					}
					else
						SendSingleLineReply(conn, Pop3ReplyType::Err, "Unrecognized POP3 command");
				}
			}
			catch (EmailServer_Disconnect const& e)
				{ SendSingleLineReply(conn, Pop3ReplyType::Err, e.what()); }
		}
		catch (Schannel::SspiErr_Acquire const& e)
		{
			m_workPool->WorkPool_LogEvent(EVENTLOG_WARNING_TYPE, e.what());
		}
		catch (CommunicationErr const&)
		{
			// Just close the connection.
		}
	}


	void Pop3ServerThread::SendSingleLineReply(Writer& writer, Pop3ReplyType replyType, Seq text)
	{
		EnsureThrow(!text.ContainsAnyByteOf("\r\n"));

		Str lineToSend;
		lineToSend.ReserveExact(4 + 1 + text.n + 2);

		if (replyType == Pop3ReplyType::Err)
			lineToSend.Set("-ERR ");
		else
			lineToSend.Set("+OK ");

		lineToSend.Add(text).Add("\r\n");
	
		writer.SetExpireMs(EmailServer_SendTimeoutMs);
		writer.Write(lineToSend);
	}


	void Pop3ServerThread::SendMultiLineReply(Writer& writer, Seq text)
	{
		// Reserving 3 for +OK, 1 for space, 5 for CRLF-dot-CRLF, 10 extra for any leading dots that need to be escaped
		Str linesToSend;
		linesToSend.ReserveExact(3 + 1 + text.n + 5 + 10);
		linesToSend.Set("+OK ");

		while (true)
		{
			Seq line = text.ReadToString("\r\n");
			text.DropBytes(2);

			if (line.FirstByte() == '.' && linesToSend.Last() == '\n')
				linesToSend.Ch('.');

			linesToSend.Add(line).Add("\r\n");

			if (!text.n)
				break;
		}

		linesToSend.Add(".\r\n");

		writer.SetExpireMs(EmailServer_SendTimeoutMs);
		writer.Write(linesToSend);
	}

}
