#include "AtIncludes.h"
#include "AtSmtpSenderThread.h"

#include "AtBCrypt.h"
#include "AtDnsQuery.h"
#include "AtImfReadWrite.h"
#include "AtNumCvt.h"
#include "AtSocketConnector.h"
#include "AtSocketReader.h"
#include "AtSocketWriter.h"
#include "AtTime.h"
#include "AtWinErr.h"


namespace At
{

	namespace
	{
		// RFC 5321, section 4.5.3.2, specifies the following ridiculous timeouts:
		//
		// - Initial 220 Message: 5 Minutes
		// - MAIL Command: 5 Minutes
		// - RCPT Command: 5 Minutes
		// - DATA Initiation: 2 Minutes
		// - Data Block: 3 Minutes (timeout for each send completion)
		// - DATA Termination: 10 Minutes
		//
		// Actually implementing this exposes us to a denial of service strategy where a receiving server
		// exploits all timeouts to near-maximum. The receiver can maintain TCP window size 1, and take three
		// minutes to acknowledge each frame (approx 1500 bytes), forcing us to send as slowly as 9 bytes
		// per second. If we send a mass mail that's 15 kB in size, the receiver can make this take 27 minutes
		// based on per-command timeouts, plus 3 minutes per 1500 bytes, for 57 minutes total. If the receiver
		// has multiple such mail exchangers, and we try up to three per delivery attempt, that's 3 hours total.
		// If we're limiting ourselves to N = { 10, 100, 1000 } concurrent senders, the receiver can completely
		// choke our sending capability for a day by arranging 8 * N = { 80, 800, 8000 } mass mail subscriptions
		// to addresses under its control. If the mass mail is 100 kB, it can take up to 227 minutes to send,
		// and the number of subscriptions per 1 day of clogging is 2 * N = { 20, 200, 2000 } instead.
		// 
		// Alternately: if we're providing a web mail service for users we don't fully trust, the user controls
		// message size, and can clog us indefinitely by just sending a number of messages equal to the number
		// of senders we're limited to.
		//
		// We implement the above timeouts, but since circumstances of senders may differ, we allow each message
		// to configure timeouts that override these defaults, as part of the message sending instruction itself.
		// That way, messages sent e.g. on behalf of semi-trusted webmail users can have much shorter timeouts.

		enum
		{
			DnsLookupTimeoutMs =  15 * 1000,
			ConnectTimeoutMs   =  30 * 1000,
			GreetingTimeoutMs  = 300 * 1000,
			EhloReplyTimeoutMs = 120 * 1000,
			StartTlsTimeoutMs  = 120 * 1000,
			AuthReplyTimeoutMs = 300 * 1000,
			MailReplyTimeoutMs = 300 * 1000,
			RcptReplyTimeoutMs = 300 * 1000,
			DataReplyTimeoutMs = 120 * 1000,
			DataDoneTimeoutMs  = 600 * 1000,
			QuitReplyTimeoutMs =   5 * 1000,

			SocketRecvTimeoutSeconds = 600,
			SocketSendTimeoutSeconds = 180,

			ReconnectSleepMs = 5 * 1000,
		};


		struct DeliveryFailure
		{
			struct Type { enum E { Temp, Perm }; };
			
			DeliveryFailure(Type::E type, Rp<SmtpSendFailure> const& failure) : m_type(type), m_failure(failure) {}

			Type::E             m_type;
			Rp<SmtpSendFailure> m_failure;
		};


		struct PermFailure : public DeliveryFailure
		{
			PermFailure(Rp<SmtpSendFailure> const& failure) : DeliveryFailure(Type::Perm, failure) {};
		};

		struct TempFailure : public DeliveryFailure
		{
			TempFailure(Rp<SmtpSendFailure> const& failure) : DeliveryFailure(Type::Temp, failure) {};
		};

	}	// anon


	void SmtpSenderThread::WorkPoolThread_ProcessWorkItem(void* pvWorkItem)
	{
		SockInit                     sockInit;
		AutoFree<SmtpSenderWorkItem> workItem { (SmtpSenderWorkItem*) pvWorkItem };
		Rp<SmtpMsgToSend>&           msg      { workItem->m_msg };
		Seq const                    content  { workItem->m_content };
		
		Vec<MailboxResult>  mailboxResults;
		SmtpTlsAssurance::E tlsAssuranceAchieved {};

		if (msg->f_pendingMailboxes.Any())
		{
			m_workPool->SmtpSender_GetSendLog().SmtpSendLog_OnAttempt(msg.Ref(), content.n);

			try
			{
				// Get SMTP sender configuration
				SmtpSenderCfg cfg { Entity::Contained };
				m_workPool->SmtpSender_GetCfg(cfg);

				// Set an overall timeout if message requests one
				Timeouts timeouts;
				if (msg->f_baseSendSecondsMax > 0)
				{
					uint64 expireMsBase { NumCast<uint64>(msg->f_baseSendSecondsMax) * 1000 };
					uint64 expireMsAdd { content.Len() / PickMax<uint64>(msg->f_nrBytesToAddOneSec, 10) };
					timeouts.SetOverallExpireMs(expireMsBase + expireMsAdd);
				}

				// Does the message contain 8-bit characters?
				bool contains8bit { content.ContainsAnyByteNotOfType(Ascii::Is7Bit) };

				// Send the message
				if (cfg.f_useRelay)
					LookupRelayAndSendMsg(cfg, timeouts, msg.Ref(), content, contains8bit, mailboxResults, tlsAssuranceAchieved);
				else
					FindMailExchangersAndSendMsg(cfg, timeouts, msg.Ref(), content, contains8bit, mailboxResults, tlsAssuranceAchieved);
			}
			catch (DeliveryFailure& e)
			{
				mailboxResults.Clear();

				Time now = Time::NonStrictNow();
				for (Str const& mailbox : msg->f_pendingMailboxes)
					if (e.m_type == DeliveryFailure::Type::Temp)
						MailboxResult_SetTempFailure_Copy(mailboxResults.Add(), now, mailbox, e.m_failure);
					else
						MailboxResult_SetPermFailure_Copy(mailboxResults.Add(), now, mailbox, e.m_failure);
			}
		}
		
		bool anyTempFailures {}, anySuccess {};
		for (MailboxResult const& result : mailboxResults)
			     if (result.f_state == SmtpDeliveryState::TempFailure) { anyTempFailures = true; }
			else if (result.f_state == SmtpDeliveryState::Success    ) { anySuccess      = true; }
			
		m_workPool->GetStore().RunTx(GetStopCtl(), typeid(*this), [&] ()
			{
				Rp<SmtpMsgToSend> txMsg { new SmtpMsgToSend(msg.Ref()) };
				SmtpDeliveryInstr::E instr = m_workPool->SmtpSender_InTx_OnDeliveryResult(txMsg.Ref(), content, mailboxResults, tlsAssuranceAchieved);

				txMsg->f_pendingMailboxes.Clear();
				for (MailboxResult const& result : mailboxResults)
					txMsg->f_mailboxResults.Add() = result;

				SmtpMsgStatus::E removeMsgStatus = SmtpMsgStatus::NonFinal_Idle;	// If unchanged, indicates the message is NOT being removed

				     if (!anyTempFailures && anySuccess)          removeMsgStatus = SmtpMsgStatus::Final_Sent;
				else if (!anyTempFailures)                        removeMsgStatus = SmtpMsgStatus::Final_Failed;
				else if (instr == SmtpDeliveryInstr::Abort)       removeMsgStatus = SmtpMsgStatus::Final_Aborted;
				else if (!txMsg->f_futureRetryDelayMinutes.Any()) removeMsgStatus = SmtpMsgStatus::Final_GivenUp;
				else
				{
					for (MailboxResult const& result : mailboxResults)
						if (result.f_state == SmtpDeliveryState::TempFailure)
							txMsg->f_pendingMailboxes.Add(result.f_mailbox);

					txMsg->f_status = SmtpMsgStatus::NonFinal_Idle;
					txMsg->f_nextAttemptTime = Time::StrictNow() + Time::FromMinutes(NumCast<uint64>(txMsg->f_futureRetryDelayMinutes.First()));
					txMsg->f_futureRetryDelayMinutes.PopFirst();
					txMsg->Update();

					txMsg->GetStore().AddPostCommitAction( [this] () { m_workPool->m_pumpTrigger.Signal(); } );
				}

				if (SmtpMsgStatus::NonFinal_Idle != removeMsgStatus)
				{
					txMsg->Remove();
					txMsg->f_status = removeMsgStatus;

					m_workPool->SmtpSender_InTx_OnMsgRemoved(txMsg.Ref());
				}

				txMsg->GetStore().AddPostCommitAction( [this, &msg, txMsg] () { msg = txMsg; } );
			} );

		m_workPool->SmtpSender_GetSendLog().SmtpSendLog_OnResult(msg.Ref(), content.n, mailboxResults, tlsAssuranceAchieved);
	}


	void SmtpSenderThread::LookupRelayAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, Seq const content, bool contains8bit,
		Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved)
	{
		EnsureThrow(cfg.f_useRelay);

		ThreadPtr<AsyncAddrLookup> aal { Thread::Create };
		aal->m_nodeName = cfg.f_relayHost;
		aal->m_serviceName.UInt(cfg.f_relayPort);
		aal->m_useHints = true;
		aal->m_hints.ai_family   = AF_UNSPEC;
		aal->m_hints.ai_socktype = SOCK_STREAM;
		aal->m_hints.ai_protocol = IPPROTO_TCP;
	
		try
		{
			aal->ApplyTimeouts(timeouts, DnsLookupTimeoutMs);
			aal->Start(GetStopCtl()).Join();
		}
		catch (Abortable::TimeExpired const&)
			{ throw TempFailure(SmtpSendFailure_RelayLookup(SmtpSendDetail::RelayLookup_LookupTimedOut, Seq())); }

		if (aal->m_rc != 0)
			throw TempFailure(SmtpSendFailure_RelayLookup(SmtpSendDetail::RelayLookup_CouldNotLookup, Str().Fun(DescribeWinErr, aal->m_rc)));

		Map<LookedUpAddr> addrs;
		ProcessAddrInfoResults(aal->m_result, cfg.f_relayHost, cfg.f_ipVerPreference, 0, addrs);

		SendAttempter sendAttempter;
		for (Map<LookedUpAddr>::ConstIt addrIt = addrs.begin(); addrIt.Any(); ++addrIt)
		{
			Rp<SmtpSenderConnection> ssc = new SmtpSenderConnection;
			ssc->m_mxa = addrIt.Ref();
			ssc->m_haveMxDomainMatch = true;

			if (SendAttempter::Result::Stop == sendAttempter.TrySendMsgToMailExchanger(
					*this, cfg, timeouts, ssc, msg, content, contains8bit, mailboxResults, tlsAssuranceAchieved))
				break;
		}

		sendAttempter.OnSuccessOrAttemptsExhausted();
	}


	void SmtpSenderThread::FindMailExchangersAndSendMsg(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpMsgToSend const& msg, Seq const content, bool contains8bit,
		Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved)
	{
		EnsureThrow(!cfg.f_useRelay);

		// Lookup destination domain mail exchangers
		// Need separate thread because lookup blocks for a while, and there's no asynchronous alternative
		ThreadPtr<AsyncMxLookup> aml { Thread::Create };
		aml->m_domain = msg.f_toDomain;
		aml->m_ipVerPreference = cfg.f_ipVerPreference;

		try
		{
			aml->ApplyTimeouts(timeouts, DnsLookupTimeoutMs);
			aml->Start(GetStopCtl()).Join();
		}
		catch (Abortable::TimeExpired const&)
			{ throw TempFailure(SmtpSendFailure_FindMx(SmtpSendDetail::FindMx_LookupTimedOut, Seq())); }

		if (aml->m_rc != 0)
			throw TempFailure(SmtpSendFailure_FindMx(SmtpSendDetail::FindMx_CouldNotLookup, Str().Fun(DescribeWinErr, aml->m_rc)));

		// Favor mail exchangers with domain match first. Then, try exchangers according to priority
		Map<LookedUpAddr> const& mxMap = aml->m_mxLookup.m_results;
		if (!mxMap.Any())
			throw TempFailure(SmtpSendFailure_FindMx(SmtpSendDetail::FindMx_LookupNoResults, Seq()));

		// Make a list of domains an MX can match to be considered a domain match
		Vec<Seq> matchDomains;
		if (Imf::IsValidDomain(msg.f_toDomain))
			matchDomains.Add(Seq(msg.f_toDomain).Trim());
		for (Str const& domain : msg.f_additionalMatchDomains)
			if (Imf::IsValidDomain(domain))
				matchDomains.Add(Seq(domain).Trim());

		// Sort MXs into domain-matching and non-matching
		Vec<LookedUpAddr const*> domainMatchMxs;
		Vec<LookedUpAddr const*> noMatchMxs;

		for (LookedUpAddr const& entry : mxMap)
		{
			bool haveMatch {};
			for (Seq domain : matchDomains)
				if (Imf::IsEqualOrSubDomainOf(domain, entry.m_dnsName))
				{
					haveMatch = true;
					break;
				}

			if (haveMatch)
				domainMatchMxs.Add(&entry);
			else
				noMatchMxs.Add(&entry);
		}

		auto describeNoMatchMxs = [&] () -> Str
			{
				Str s = "The following non-matching exchangers were not tried: ";
				s.Add(noMatchMxs[0]->m_dnsName);
				if (noMatchMxs.Len() > 1) s.Add(", ").Add(noMatchMxs[1]->m_dnsName);
				if (noMatchMxs.Len() > 2) s.Add(", ").Add(noMatchMxs[2]->m_dnsName);
				if (noMatchMxs.Len() > 3) s.Add(", ...");
				return s;
			};

		if (!domainMatchMxs.Any() && msg.f_tlsRequirement == SmtpTlsAssurance::Tls_DomainMatch)
			throw TempFailure(SmtpSendFailure_FindMx(SmtpSendDetail::FindMx_DomainMatchRequired, describeNoMatchMxs()));

		// Loop through mail exchangers to send
		SendAttempter sendAttempter;
		sizet nrMxs { domainMatchMxs.Len() + noMatchMxs.Len() };
		for (sizet i=0; i!=nrMxs; ++i)
		{
			// What mail exchanger's turn is it?
			LookedUpAddr const* mxa {};
			bool haveMxDomainMatch {};

			if (i < domainMatchMxs.Len())
			{
				mxa = domainMatchMxs[i];
				haveMxDomainMatch = true;
			}
			else
			{
				if (msg.f_tlsRequirement == SmtpTlsAssurance::Tls_DomainMatch)
				{
					if (noMatchMxs.Any())
						sendAttempter.m_failDescSuffix.SetAdd("The message requires TLS with domain match. ", describeNoMatchMxs());
					break;
				}

				sizet noMatchMxIndex = i - domainMatchMxs.Len();
				mxa = noMatchMxs[noMatchMxIndex];
			}

			// Try sending to this mail exchanger address
			Rp<SmtpSenderConnection> ssc = new SmtpSenderConnection;
			ssc->m_mxa = *mxa;
			ssc->m_haveMxDomainMatch = haveMxDomainMatch;

			if (SendAttempter::Result::Stop == sendAttempter.TrySendMsgToMailExchanger(
					*this, cfg, timeouts, ssc, msg, content, contains8bit, mailboxResults, tlsAssuranceAchieved))
				break;
		}

		sendAttempter.OnSuccessOrAttemptsExhausted();
	}


	SmtpSenderThread::SendAttempter::Result SmtpSenderThread::SendAttempter::TrySendMsgToMailExchanger(
		SmtpSenderThread& outer, SmtpSenderCfg const& cfg, Timeouts const& timeouts, Rp<SmtpSenderConnection> const& ssc,
		SmtpMsgToSend const& msg, Seq const content, bool contains8bit, Vec<MailboxResult>& mailboxResults, SmtpTlsAssurance::E& tlsAssuranceAchieved)
	{
		try
		{
			mailboxResults.Clear();
			outer.SendMsgToMailExchanger(cfg, timeouts, ssc, msg, content, contains8bit, mailboxResults);
			if (tlsAssuranceAchieved < ssc->m_tlsAssurance)
				tlsAssuranceAchieved = ssc->m_tlsAssurance;
			
			// No exception thrown indicates no blanket failure for all mailboxes being delivered to.
			// It does not guarantee successful delivery to any mailbox - all of them may have failed
			// at RCPT TO: - but in this case we are satisfied that the result is as good as it gets.
			m_haveGoodEnoughResult = true;
			return Result::Stop;
		}
		catch (DeliveryFailure const& e)
		{
			if (e.m_type != DeliveryFailure::Type::Temp)
				throw;

			if (e.m_failure->f_detail == SmtpSendDetail::Connect)
			{
				if (!m_firstConnFailure.Any())
					m_firstConnFailure = e.m_failure;
			}
			else
			{
				if (!m_firstTempFailure.Any())
					m_firstTempFailure = e.m_failure;
			
				if (++m_nrTempDeliveryFailures >= Smtp_AttemptMaxTempFailures)
					return Result::Stop;
			}

			if (outer.StopEvent().IsSignaled())
				return Result::Stop;
		}

		// Permanent blanket delivery failures fall through to be handled by message dispatcher code in WorkPoolThread_ProcessWorkItem.
		// Other exceptions fall through and cause the SMTP sender work pool to shutdown.
		return Result::TryMore;
	}


	void SmtpSenderThread::SendAttempter::OnSuccessOrAttemptsExhausted()
	{
		if (!m_haveGoodEnoughResult)
		{
			if (m_firstTempFailure.Any())
			{
				if (m_failDescSuffix.Any())
					m_firstTempFailure->f_desc.IfAny("\r\n").Add(m_failDescSuffix);
				throw TempFailure(m_firstTempFailure);
			}
			
			if (m_firstConnFailure.Any())
			{
				if (m_failDescSuffix.Any())
					m_firstConnFailure->f_desc.IfAny("\r\n").Add(m_failDescSuffix);
				throw TempFailure(m_firstConnFailure);
			}
			
			EnsureThrow(!"Unexpected reason for unsatisfactory delivery result");
		}
	}


	namespace SmtpSenderThreadInternal
	{
		SmtpTlsAssurance::E InitialTlsAssuranceGoal(SmtpMsgToSend const& msg, SmtpTlsAssurance::E tlsAssuranceRqmt)
		{
			// In our current implementation, the decision about verifying TLS host identity is made in advance.
			// If we are verifying the TLS host identity, we let Schannel validate server credentials.
			// If we are not verifying the TLS host identity, we enable manual credential validation (and then do nothing).
			// If we overshoot expectations - try to verify TLS host identity but it fails - we have to reconnect.
			// However, if we reconnect, the host may have already temporarily blocked us using graylisting.
			// Therefore, if the message does not need TLS with host authentication, at some point in our retries
			// we have to make our initial attempt without host authentication, to increase the odds it will go through.

			if (tlsAssuranceRqmt <= SmtpTlsAssurance::Tls_NoHostAuth)
			{
				sizet nrResultsWithTlsAuthFail {};
				for (sizet i=msg.f_mailboxResults.Len(); i!=0; )
				{
					MailboxResult const& mr = msg.f_mailboxResults[--i];
					if (SmtpDeliveryState::Success != mr.f_state)
					{
						bool resultHasTlsAuthFail {};
						SmtpSendFailure const* f = mr.f_failure.Ptr();
						while (nullptr != f)
						{
							if (f->f_tlsAssurance > SmtpTlsAssurance::Tls_NoHostAuth)
								return SmtpTlsAssurance::Tls_ExactMatch;

							if (SmtpSendDetail::Tls_Sspi_ServerNotAuthenticated == f->f_detail)
								resultHasTlsAuthFail = true;

							f = f->f_prevFailure.DynamicCast<SmtpSendFailure>();
						}

						if (resultHasTlsAuthFail)
							if (2 == ++nrResultsWithTlsAuthFail)
								return SmtpTlsAssurance::Tls_NoHostAuth;
					}
				}
			}

			return SmtpTlsAssurance::Tls_ExactMatch;
		}


		void SendData(SmtpSenderConnection& ssc, SmtpSendStage::E stage, Seq data, Rp<SmtpSendFailure> const& prevFailure)
		{
			try { ssc.m_sc->m_conn.Write(data); }
			catch (CommunicationErr const& e) { throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Send, e.what(), prevFailure)); }
		}


		void ReadReply(SmtpServerReply& reply, SmtpSenderConnection& ssc, SmtpSendStage::E stage, Rp<SmtpSendFailure> const& prevFailure)
		{
			try
			{
				sizet replyBytes = 0;
				while (true)
				{
					bool lastLine = false;

					ssc.m_sc->m_conn.Read( [&] (Seq& avail) -> Reader::ReadInstr
						{
							Seq const line { avail.ReadToString("\r\n") };
							if (avail.n)
							{
								if (line.n < 3)
									throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_PrematureEndOfLine, Seq(), prevFailure));
								if (!Ascii::IsDecDigit(line.p[0]) || !Ascii::IsDecDigit(line.p[1]) || !Ascii::IsDecDigit(line.p[2]))
									throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_UnrecognizedCodeFormat, Seq(), prevFailure));

								if (line.n == 3 || line.p[3] == ' ')
									lastLine = true;
								else if (line.p[3] != '-')
									throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_UnrecognizedLineSeparator, Seq(), prevFailure));

								Seq lineReader = line;
								uint32 code = lineReader.ReadNrUInt32Dec(999);
								lineReader.DropByte();

								if (!reply.m_code.Any())
								{
									reply.m_code = SmtpReplyCode(code);

									// Read enhanced status code, if any
									Seq enhReader = lineReader;
									reply.m_enhStatus = SmtpEnhStatus::Read(enhReader);
								}
								else
								{
									if (reply.m_code.Value() != code)
										throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_InconsistentCode,
											Str("Code on first line: ").UInt(reply.m_code.Value()).Add(", subsequent line: ").UInt(code), prevFailure));
								}
			
								reply.m_lines.Add().Set(lineReader);
								avail.DropBytes(2);
								return Reader::ReadInstr::Done;
							}

							return Reader::ReadInstr::NeedMore;
						} );

					if (lastLine)
						break;
			
					if (replyBytes > Smtp_MaxServerReplyBytes)
						throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_MaximumLengthExceeded, Seq(), prevFailure));
				}
			}
			catch (CommunicationErr const& e) { throw TempFailure(SmtpSendFailure_NoCode(ssc, stage, SmtpSendDetail::Reply_CouldNotReceive, e.what(), prevFailure)); }
		}

	}

	using namespace SmtpSenderThreadInternal;


	// TLS failure handling is based on the following experience. On November 30, 2015, a batch of security notifications was sent to 36,000 recipients.
	// The initial sender implementation did not implement handling for TLS failure, and just kept retrying on normal message retry schedule.
	// The following is the complete set of TLS errors encountered, with the number of delivery attempts that encountered each:
	//
	//    966: -2146893048 = 0x80090308 = SEC_E_INVALID_TOKEN          = The token supplied to the function is invalid
	//    144: -2146893018 = 0x80090326 = SEC_E_ILLEGAL_MESSAGE        = The message received was unexpected or badly formatted.
	//
	//         These were received repeatedly without improvement for each recipient checked.
	//         -> If the message does not request TLS, retry without attempting TLS.
	//            If the message requests TLS, keep retrying until eventually giving up.
	// 
	//  46545: -2146893022 = 0x80090322 = SEC_E_WRONG_PRINCIPAL        = The target principal name is incorrect.
	//  33800: -2146893019 = 0x80090325 = SEC_E_UNTRUSTED_ROOT         = The certificate chain was issued by an authority that is not trusted.
	//   2837: -2146893016 = 0x80090328 = SEC_E_CERT_EXPIRED           = The received certificate has expired.
	//     74: -2146869244 = 0x80096004 = TRUST_E_CERT_SIGNATURE       = The signature of the certificate cannot be verified.
	//
	//         These were received repeatedly without improvement for each recipient checked.
	//         -> If the message does not request TLS with host authentication, retry with TLS, but without host authentication.
	//            If the message requests TLS with host authentication, keep retrying until eventually giving up.
	//
	//      3: -2146893041 = 0x8009030F = SEC_E_MESSAGE_ALTERED        = The message or signature supplied for verification has been altered
	//      5: -2146893023 = 0x80090321 = SEC_E_INCOMPLETE_CREDENTIALS = The credentials supplied were not complete, and could not be verified. The context could not be initialized.
	//
	//         These were received one time each for different recipients. All succeeded in next attempts with TLS.
	//         -> Retry with TLS.

	void SmtpSenderThread::SendMsgToMailExchanger(SmtpSenderCfg const& cfg, Timeouts const& timeouts, Rp<SmtpSenderConnection> const& ssc,
		SmtpMsgToSend const& msg, Seq const content, bool contains8bit, Vec<MailboxResult>& mailboxResults)
	{
		SmtpTlsAssurance::E tlsAssuranceRqmt;
		if (cfg.f_useRelay)
			tlsAssuranceRqmt = (SmtpTlsAssurance::E) PickMax(cfg.f_relayTlsRequirement, msg.f_tlsRequirement);
		else
			tlsAssuranceRqmt = msg.f_tlsRequirement;

		SmtpTlsAssurance::E tlsAssuranceGoal = InitialTlsAssuranceGoal(msg, tlsAssuranceRqmt);

		enum { MaxReconnectsDueToLikelyDhIssue = 2 };
		sizet nrConnectAttempts {};
		sizet nrReconnectsDueToLikelyDhIssue {};

		EnsureThrow(ssc->m_mxa.m_sa.IsIp4Or6());
		Vec<Str> const& localInterfaces = (ssc->m_mxa.m_sa.IsIp4() ? cfg.f_localInterfacesIp4 : cfg.f_localInterfacesIp6);
		sizet localInterfaceIndex {};

		Rp<SmtpSendFailure> prevFailure;

	Reconnect:
		EnsureThrow(tlsAssuranceGoal >= tlsAssuranceRqmt);

		if (++nrConnectAttempts > 1)
			AbortableSleep(ReconnectSleepMs);

		ssc->m_sc.Set(new SocketConnection);
		ssc->m_localAddr.Clear();
		ssc->m_tlsAssurance = SmtpTlsAssurance::Unknown;
		SocketConnection& sc = ssc->m_sc.Ref();

		try
		{
			SockAddr saFinal(ssc->m_mxa.m_sa);
			if (cfg.f_useRelay)
				saFinal.SetPort((uint16) cfg.f_relayPort);
			else
				saFinal.SetPort(25);

			auto configureSocket = [&] (Socket& s)
				{
					if (localInterfaceIndex < localInterfaces.Len())
					{
						ssc->m_localAddr = localInterfaces[localInterfaceIndex];
						WinStr wzLocalInterface { ssc->m_localAddr };
						SockAddr saLocal;
						if (saFinal.IsIp4())
							saLocal.ParseIp4(wzLocalInterface.Z());
						else
							saLocal.ParseIp6(wzLocalInterface.Z());
						saLocal.SetPort(0);
						s.Bind(saLocal);
					}
				};

			SocketConnector connector;
			connector.SetStopCtl(*this);
			connector.ApplyTimeouts(timeouts, ConnectTimeoutMs);
			sc.m_sk.SetSocket(connector.ConnectToAddr(saFinal, configureSocket));
			sc.m_sk.SetNoDelay(true);
		}
		catch (Abortable::Err       const& e) { throw TempFailure(SmtpSendFailure_Connect(SmtpSendDetail::Connect, ssc.Ref(), e.what(), prevFailure)); }
		catch (SocketConnector::Err const& e) { throw TempFailure(SmtpSendFailure_Connect(SmtpSendDetail::Connect, ssc.Ref(), e.what(), prevFailure)); }

		ssc->m_tlsAssurance = SmtpTlsAssurance::NoTls;
		sc.m_sk.SetRecvTimeout(SocketRecvTimeoutSeconds);
		sc.m_sk.SetSendTimeout(SocketSendTimeoutSeconds);
		sc.OnSocketConnected();
		sc.m_conn.SetStopCtl(*this);

		if (cfg.f_transcriptToDomains.Any())
		{
			auto criterion = [&msg] (Str const& addr) -> bool { Seq a = addr; return a.EqualExact("*") || a.EqualInsensitive(msg.f_toDomain); };
			if (cfg.f_transcriptToDomains.Contains(criterion))
			{
				Rp<Transcriber> transcriber = TextFileTranscriber::Start("SmtpSender", sc.m_sk);
				sc.m_reader.SetTranscriber(transcriber);
				sc.m_writer.SetTranscriber(transcriber);
				sc.m_conn.SetTranscriber(transcriber);
			}
		}

		Str  fromDomainName     { Imf::ExtractDomainFromEmailAddress(msg.f_fromAddress) };
		Str  ourName            { m_workPool->SmtpSender_SenderComputerName(fromDomainName) };
		bool usingImplicitTls   { cfg.f_useRelay && cfg.f_relayImplicitTls };
		bool usingTlsHostAuth   {};
		bool usingTlsExactMatch {};

	RestartAtGreetingAfterImplicitTls:
		// Expect greeting
		if (!usingImplicitTls || sc.m_conn.TlsStarted())
		{
			sc.m_conn.ApplyTimeouts(timeouts, GreetingTimeoutMs);

			SmtpServerReply greeting;
			ReadReply(greeting, ssc.Ref(), SmtpSendStage::Greeting, prevFailure);

			if (!greeting.m_code.IsPositiveCompletion())
			{
				if (greeting.m_code.IsNegativeTransient())
					throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Greeting, SmtpSendDetail::Greeting_Unexpected, greeting, Seq(), prevFailure));

				Rp<SmtpSendFailure> failure = SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Greeting, SmtpSendDetail::Greeting_SessionRefused, greeting, Seq(), prevFailure);

				// The MX may be blocking our sending IP. Try another if we have more
				if (localInterfaceIndex + 1 < localInterfaces.Len())
				{
					prevFailure = failure;
					++localInterfaceIndex;
					goto Reconnect;
				}

				throw PermFailure(failure);
			}
		}	

	RestartAtEhloAfterStartTls:
		bool l_supports_startTls       {};
		bool l_supports_8BitMime       {};
		bool l_supports_size           {};
		bool l_supports_auth           {};
		bool l_supports_auth_plain     {};
		bool l_supports_auth_crammd5   {};
		bool l_supports_auth_digestmd5 {};
		uint64 l_size_max              {};

		// Send EHLO
		if (!usingImplicitTls || sc.m_conn.TlsStarted())
		{
			sc.m_conn.ApplyTimeouts(timeouts, EhloReplyTimeoutMs);

			Str ehlo(Str("EHLO ").Add(ourName).Add("\r\n"));
			SendData(ssc.Ref(), SmtpSendStage::Cmd_Ehlo, ehlo, prevFailure);

			SmtpServerReply ehloReply;
			ReadReply(ehloReply, ssc.Ref(), SmtpSendStage::Cmd_Ehlo, prevFailure);
			if (!ehloReply.m_code.IsPositiveCompletion())
				throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Ehlo, SmtpSendDetail::Ehlo_UnexpectedReply, ehloReply, Seq(), prevFailure));

			for (Str const& line : ehloReply.m_lines.GetSlice(1))
			{
				Seq lineReader = line;
				SeqLower const token { lineReader.ReadToFirstByteOfType(Ascii::IsWhitespace) };

				lineReader.DropToFirstByteNotOfType(Ascii::IsWhitespace);

					 if (token.EqualInsensitive("starttls")) { l_supports_startTls = true; }
				else if (token.EqualInsensitive("8bitmime")) { l_supports_8BitMime = true; }
				else if (token.EqualInsensitive("size"    )) { l_supports_size     = true; l_size_max = lineReader.ReadNrUInt64Dec(); }
				else if (token.EqualInsensitive("auth"    ))
				{
					l_supports_auth = true;

					while (lineReader.n)
					{
						SeqLower const method { lineReader.ReadToFirstByteOfType(Ascii::IsWhitespace) };
						
							 if (method.EqualInsensitive("PLAIN"      )) l_supports_auth_plain     = true;
						else if (method.EqualInsensitive("CRAM-MD5"   )) l_supports_auth_crammd5   = true;
						else if (method.EqualInsensitive("DIGEST-MD5" )) l_supports_auth_digestmd5 = true;

						lineReader.DropToFirstByteNotOfType(Ascii::IsWhitespace);
					}
				}
			}
		}

		// Start TLS if not started yet (and if the server supports it)
		if (!sc.m_conn.TlsStarted() && tlsAssuranceGoal > SmtpTlsAssurance::NoTls)
		{
			if (!usingImplicitTls && !l_supports_startTls)
			{
				if (tlsAssuranceRqmt > SmtpTlsAssurance::NoTls)
					throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_NotAvailable, Seq(), prevFailure));
			}
			else
			{
				bool okToBeginTls {};

				if (usingImplicitTls)
					okToBeginTls = true;
				else
				{
					// Send STARTTLS
					sc.m_conn.ApplyTimeouts(timeouts, StartTlsTimeoutMs);

					SendData(ssc.Ref(), SmtpSendStage::Tls, "STARTTLS\r\n", prevFailure);

					SmtpServerReply startTlsReply;
					ReadReply(startTlsReply, ssc.Ref(), SmtpSendStage::Tls, prevFailure);
					if (startTlsReply.m_code.IsPositiveCompletion())
						okToBeginTls = true;
					else
					{
						// TLS rejected by server
						if (tlsAssuranceRqmt > SmtpTlsAssurance::NoTls)
							throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_StartTlsRejected, startTlsReply, Seq(), prevFailure));
					}
				}

				if (okToBeginTls)
				{
					m_workPool->SmtpSender_AddSchannelCerts(ourName, sc.m_conn);
					sc.m_conn.InitCred(ProtoSide::Client);
					sc.m_conn.SetServerName(ssc->m_mxa.m_dnsName);

					if (tlsAssuranceGoal <= SmtpTlsAssurance::Tls_NoHostAuth)
					{
						sc.m_conn.SetManualCredValidation(true);
						usingTlsHostAuth   = false;
						usingTlsExactMatch = false;
					}
					else
					{
						usingTlsHostAuth = true;
						usingTlsExactMatch = cfg.f_useRelay && Seq(ssc->m_mxa.m_dnsName).EqualInsensitive(cfg.f_relayHost);
					}

					try { sc.m_conn.StartTls(); }
					catch (Schannel::SspiErr_LikelyDhIssue_TryRestart const& e)
					{
						Rp<SmtpSendFailure> failure = SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Sspi_LikelyDhIssue, e.what(), prevFailure);

						if (nrReconnectsDueToLikelyDhIssue < MaxReconnectsDueToLikelyDhIssue)
						{
							prevFailure = failure;
							++nrReconnectsDueToLikelyDhIssue;
							goto Reconnect;
						}

						if (tlsAssuranceRqmt <= SmtpTlsAssurance::NoTls)
						{
							prevFailure = failure;
							tlsAssuranceGoal = SmtpTlsAssurance::NoTls;
							goto Reconnect;
						}

						throw TempFailure(failure);
					}
					catch (Schannel::SspiErr const& e)
					{
						if (e.m_code == SEC_E_INVALID_TOKEN || e.m_code == SEC_E_ILLEGAL_MESSAGE)
						{
							Rp<SmtpSendFailure> failure = SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Sspi_InvalidToken_IllegalMsg, e.what(), prevFailure);

							if (tlsAssuranceRqmt <= SmtpTlsAssurance::NoTls)
							{
								prevFailure = failure;
								tlsAssuranceGoal = SmtpTlsAssurance::NoTls;
								goto Reconnect;
							}

							throw TempFailure(failure);
						}

						if (e.m_code == SEC_E_WRONG_PRINCIPAL || e.m_code == SEC_E_UNTRUSTED_ROOT || e.m_code == SEC_E_CERT_EXPIRED || e.m_code == TRUST_E_CERT_SIGNATURE)
						{
							Rp<SmtpSendFailure> failure = SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Sspi_ServerNotAuthenticated, e.what(), prevFailure);

							if (tlsAssuranceRqmt <= SmtpTlsAssurance::Tls_NoHostAuth)
							{
								prevFailure = failure;
								tlsAssuranceGoal = SmtpTlsAssurance::Tls_NoHostAuth;
								goto Reconnect;
							}

							throw TempFailure(failure);
						}

						if (e.m_code == SEC_E_INVALID_PARAMETER && e.m_errLoc == Schannel::ErrLoc::IASC_Cont && Seq(e.m_alertSent).EndsWithExact("\x02\x02\x28"))
						{
							// 2020-12-19: Identified an MX that uses TLS 1.0, with a 512-bit RSA key, and selects the cipher suite TLS_RSA_WITH_AES_256_CBC_SHA.
							// Testing with OpenSSL shows that Schannel would work with TLS 1.0 and a 512-bit RSA key if e.g. an ECDHE cipher suite was selected.
							// However, when a 512-bit RSA key is used with the TLS_RSA_WITH_AES_256_CBC_SHA cipher suite (OpenSSL cipher name: "AES256-SHA"),
							// then a subsequent call to InitializeSecurityContext fails with SEC_E_INVALID_PARAMETER and returns the alert: 15 03 01 00 02 02 28.
							// This is AlertLevel: fatal (2), AlertDescription: handshake_failure (0x28 == 40).
							//
							// In testing, it was not possible to make this work by e.g. permitting the use of weak ciphers in the SCHANNEL_CRED structure.
							// Therefore, the only remedy at this time appears to be to downgrade to not use TLS.
							//
							// OpenSSL command to create a self-signed 512-bit RSA certificate:
							// openssl req -x509 -newkey rsa:512 -keyout key.pem -out server.pem -days 365 -subj /CN=localhost -nodes
							//
							// OpenSSL command to run test server mimicking the problematic server:
							// @openssl s_server -key key.pem -debug -www -msg -cipher "AES256-SHA:@SECLEVEL=0" -no_tls1_1 -no_tls1_2 -no_tls1_3

							Rp<SmtpSendFailure> failure = SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Sspi_Other, e.what(), prevFailure);

							if (tlsAssuranceRqmt <= SmtpTlsAssurance::NoTls)
							{
								prevFailure = failure;
								tlsAssuranceGoal = SmtpTlsAssurance::NoTls;
								goto Reconnect;
							}

							throw TempFailure(failure);
						}

						throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Sspi_Other, e.what(), prevFailure));
					}
					catch (CommunicationErr const& e) { throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_Communication, e.what(), prevFailure)); }

					if (usingImplicitTls)
						goto RestartAtGreetingAfterImplicitTls;
					else
						goto RestartAtEhloAfterStartTls;
				}
			}
		}

		// Check TLS assurance achieved
			 if (!sc.m_conn.TlsStarted())   ssc->m_tlsAssurance = SmtpTlsAssurance::NoTls;
		else if (!usingTlsHostAuth)         ssc->m_tlsAssurance = SmtpTlsAssurance::Tls_NoHostAuth;
		else if (!ssc->m_haveMxDomainMatch) ssc->m_tlsAssurance = SmtpTlsAssurance::Tls_AnyServer;
		else if (!usingTlsExactMatch)       ssc->m_tlsAssurance = SmtpTlsAssurance::Tls_DomainMatch;
		else                                ssc->m_tlsAssurance = SmtpTlsAssurance::Tls_ExactMatch;

		if (ssc->m_tlsAssurance < tlsAssuranceRqmt)
			throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Tls, SmtpSendDetail::Tls_RequiredAssuranceNotAchieved, Seq(), prevFailure));

		// Check capabilities here, after potentially negotiating TLS, rather than relying on capabilities received in plaintext
		if (contains8bit && !l_supports_8BitMime && cfg.f_respectNo8BitMime)
			throw PermFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Capabilities, SmtpSendDetail::Capabilities_8BitMimeRequired, Seq(), prevFailure));

		// SIZE supported? Is there a max size?
		if (l_supports_size && l_size_max > 0 && content.Len() > l_size_max)
			throw PermFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Capabilities, SmtpSendDetail::Capabilities_Size,
				Str("Destination MX message size limit: ").UInt(l_size_max).Add(" bytes, size of message: ").UInt(content.Len()).Add(" bytes"), prevFailure));

		// Authenticate?
		if (cfg.f_useRelay && cfg.f_relayAuthType != MailAuthType::None)
		{
			if (!l_supports_auth)
				throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_AuthCommandNotSupported, Seq(), prevFailure));

			uint64 methodToUse = cfg.f_relayAuthType;
			if (methodToUse == MailAuthType::UseSuitable)
			{
				if (l_supports_auth_plain && ssc->m_tlsAssurance == SmtpTlsAssurance::Tls_ExactMatch)
					methodToUse = MailAuthType::AuthPlain;
				else if (l_supports_auth_crammd5)
					methodToUse = MailAuthType::AuthCramMd5;
				else
					throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_NoSuitableAuthMechanism, Seq(), prevFailure));
			}
				
			if (methodToUse == MailAuthType::AuthPlain)
			{
				if (!l_supports_auth_plain)
					throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_CfgAuthMechNotSupported, Seq(), prevFailure));

				PerformAuthPlain(cfg, timeouts, ssc.Ref(), prevFailure);
			}
			else if (methodToUse == MailAuthType::AuthCramMd5)
			{
				if (!l_supports_auth_crammd5)
					throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_CfgAuthMechNotSupported, Seq(), prevFailure));

				PerformAuthCramMd5(cfg, timeouts, ssc.Ref(), prevFailure);
			}
			else
				throw TempFailure(SmtpSendFailure_NoCode(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_CfgAuthMechUnrecognized, Seq(), prevFailure));

			SmtpServerReply authReply;
			ReadReply(authReply, ssc.Ref(), SmtpSendStage::Cmd_Auth, prevFailure);
			if (!authReply.m_code.IsPositiveCompletion())
			{
					 if (authReply.m_code.IsNegativePermanent()) throw PermFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_Rejected, authReply, Seq(), prevFailure));
				else if (authReply.m_code.IsNegativeTransient()) throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_Rejected, authReply, Seq(), prevFailure));
				else                                             throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_UnexpectedReply, authReply, Seq(), prevFailure));
			}
		}
	
		// Send MAIL FROM
		{
			sc.m_conn.ApplyTimeouts(timeouts, MailReplyTimeoutMs);

			Str mailFrom;
			mailFrom.ReserveExact(50 + msg.f_fromAddress.Len());
			mailFrom.Add("MAIL FROM:<").Add(msg.f_fromAddress).Add(">");
			if (l_supports_size)
				mailFrom.Add(" SIZE=").UInt(content.Len());
			if (l_supports_8BitMime)
				mailFrom.Add(If(contains8bit, char const*, " BODY=8BITMIME", " BODY=7BIT"));
			mailFrom.Add("\r\n");
			SendData(ssc.Ref(), SmtpSendStage::Cmd_MailFrom, mailFrom, prevFailure);

			SmtpServerReply mailReply;
			ReadReply(mailReply, ssc.Ref(), SmtpSendStage::Cmd_MailFrom, prevFailure);
			if (!mailReply.m_code.IsPositiveCompletion())
			{
				if (mailReply.m_code.IsNegativePermanent())
				{
					Rp<SmtpSendFailure> failure = SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_MailFrom, SmtpSendDetail::MailFrom_Rejected, mailReply, Seq(), prevFailure);

					// The MX may be blocking our sending IP. Do we have more sending IPs we can try?
					if (localInterfaceIndex + 1 < localInterfaces.Len())
					{
						// Examples:
						// "550 5.7.1 Unfortunately, messages from [XX.XX.XX.XX] weren't sent. Please contact your Internet service provider since part of their network is on our block list &lpar;S3140&rpar;. You can also refer your provider to http://mail.live.com/mail/troubleshooting.aspx#errors. [XXXXXXXXXXXXX.XXXXXXXXX.prod.protection.outlook.com]"
						// "553 5.3.0 flpd568 DNSBL:ATTRBL 521< XX.XX.XX.XX >_is_blocked.For assistance forward this error to abuse_rbl@abuse-att.net"

						auto lineWithBlock = [] (Str const& x) -> bool { return Seq(x).ContainsString("block", CaseMatch::Insensitive); };
						uint const code = mailReply.m_code.Value();
						if (code == 550 || code == 553 || mailReply.m_lines.Contains(lineWithBlock))
						{
							prevFailure = failure;
							++localInterfaceIndex;
							goto Reconnect;
						}
					}

					throw PermFailure(failure);
				}
				else if (mailReply.m_code.IsNegativeTransient()) throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_MailFrom, SmtpSendDetail::MailFrom_Rejected, mailReply, Seq(), prevFailure));
				else                                             throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_MailFrom, SmtpSendDetail::MailFrom_UnexpectedReply, mailReply, Seq(), prevFailure));
			}
		}
	
		// Send RCPT TO
		bool anyRcptAccepted {};
		for (Str const& mailbox : msg.f_pendingMailboxes)
		{
			sc.m_conn.ApplyTimeouts(timeouts, RcptReplyTimeoutMs);
			
			Str rcptTo(Str("RCPT TO:<").Add(mailbox).Ch('@').Add(msg.f_toDomain).Add(">\r\n"));
			SendData(ssc.Ref(), SmtpSendStage::Cmd_RcptTo, rcptTo, prevFailure);

			SmtpServerReply rcptReply;
			ReadReply(rcptReply, ssc.Ref(), SmtpSendStage::Cmd_RcptTo, prevFailure);

			Time now = Time::NonStrictNow();
			if (rcptReply.m_code.IsPositiveCompletion())
			{
				MailboxResult_SetSuccess(mailboxResults.Add(), now, mailbox, Str::From(ssc->m_mxa), sc.m_sk.LocalAddr());
				anyRcptAccepted = true;
			}
			else if (rcptReply.m_code.IsNegativeTransient()) MailboxResult_SetTempFailure_Move(mailboxResults.Add(), now, mailbox, SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_RcptTo, SmtpSendDetail::RcptTo_Rejected, rcptReply, Seq(), prevFailure));
			else if (rcptReply.m_code.IsNegativePermanent()) MailboxResult_SetPermFailure_Move(mailboxResults.Add(), now, mailbox, SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_RcptTo, SmtpSendDetail::RcptTo_Rejected, rcptReply, Seq(), prevFailure));
			else
				throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_RcptTo, SmtpSendDetail::RcptTo_UnexpectedReply, rcptReply, Seq(), prevFailure));
		}

		if (anyRcptAccepted)
		{
			// Send DATA
			{
				sc.m_conn.ApplyTimeouts(timeouts, DataReplyTimeoutMs);
				SendData(ssc.Ref(), SmtpSendStage::Cmd_Data, "DATA\r\n", prevFailure);

				SmtpServerReply dataReply;
				ReadReply(dataReply, ssc.Ref(), SmtpSendStage::Cmd_Data, prevFailure);
				if (!dataReply.m_code.IsPositiveIntermediate())
				{
					if (dataReply.m_code.IsNegativePermanent()) throw PermFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Data, SmtpSendDetail::Data_Rejected, dataReply, Seq(), prevFailure));
					if (dataReply.m_code.IsNegativeTransient()) throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Data, SmtpSendDetail::Data_Rejected, dataReply, Seq(), prevFailure));
					else                                        throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Cmd_Data, SmtpSendDetail::Data_UnexpectedReply, dataReply, Seq(), prevFailure));
				}
			}

			// Send message content
			{
				// Escape dots in content, if needed, before sending
				Seq orig       { content };
				Seq dataToSend { orig };
				Str escaped;

				if (orig.n)
				{
					Seq remainder     { orig };
					Seq chunk         { remainder.ReadToString("\r\n.") };
					bool escapeStart  { (chunk.n && chunk.p[0] == '.') };
					bool escapeMiddle { (remainder.n != 0) };
			
					if (escapeStart || escapeMiddle)
					{
						if (escapeStart)
							escaped.Add(".");
					
						escaped.Add(chunk);

						if (escapeMiddle)
							do
							{
								escaped.Add("\r\n..");
								remainder.DropBytes(3);
								escaped.Add(remainder.ReadToString("\r\n."));
							}
							while (remainder.n);
				
						dataToSend = escaped;
					}
				}

				// Set timeout
				if (!timeouts.m_overallTimeout)
				{
					uint64 expireMsBase { DataDoneTimeoutMs };
					uint64 expireMsAdd  { dataToSend.n / 10 };
					sc.m_conn.SetExpireMs(expireMsBase + expireMsAdd);
				}
		
				// Send message content
				SendData(ssc.Ref(), SmtpSendStage::Content, dataToSend, prevFailure);

				Seq terminator("\r\n.\r\n");
				if (dataToSend.n && dataToSend.EndsWithExact("\r\n"))
					terminator.DropBytes(2);
		
				SendData(ssc.Ref(), SmtpSendStage::Content, terminator, prevFailure);
			}
	
			// Wait content reply
			{
				SmtpServerReply contentReply;
				ReadReply(contentReply, ssc.Ref(), SmtpSendStage::Content, prevFailure);
				if (contentReply.m_code.IsPositiveCompletion())
				{
					Time now = Time::NonStrictNow();
					for (MailboxResult& result : mailboxResults)
						if (SmtpDeliveryState::Success == result.f_state)
							result.f_time = now;
				}
				else
				{
						 if (contentReply.m_code.IsNegativePermanent()) throw PermFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Content, SmtpSendDetail::Content_Rejected, contentReply, Seq(), prevFailure));
					else if (contentReply.m_code.IsNegativeTransient()) throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Content, SmtpSendDetail::Content_Rejected, contentReply, Seq(), prevFailure));
					else                                                throw TempFailure(SmtpSendFailure_Reply(ssc.Ref(), SmtpSendStage::Content, SmtpSendDetail::Content_UnexpectedReply, contentReply, Seq(), prevFailure));
				}
			}
		}
	
		// Message sending completed, ignore subsequent errors
		try
		{
			sc.m_conn.SetExpireMs(QuitReplyTimeoutMs);

			SendData(ssc.Ref(), SmtpSendStage::Cmd_Quit, "QUIT\r\n", prevFailure);
		
			SmtpServerReply quitReply;
			ReadReply(quitReply, ssc.Ref(), SmtpSendStage::Cmd_Quit, prevFailure);
		}
		catch (DeliveryFailure  const&) {}
		catch (ExecutionAborted const&) {}	// Delivery has succeeded, so allow this to be recorded. React to stop event at next opportunity.
	}


	void SmtpSenderThread::PerformAuthPlain(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpSenderConnection& ssc, Rp<SmtpSendFailure> const& prevFailure)
	{
		EnsureThrow(cfg.f_useRelay);
		EnsureThrow(cfg.f_relayAuthType != MailAuthType::None);

		ssc.m_sc->m_conn.ApplyTimeouts(timeouts, AuthReplyTimeoutMs);
		
		Str authCmd { "AUTH PLAIN " };

		{
			Str pwPlain;
			if (cfg.f_relayPassword.Any())
				pwPlain = Crypt::UnprotectData(cfg.f_relayPassword, CRYPTPROTECT_UI_FORBIDDEN);

			Seq nullByte { "", 1 };
			Str authData = Str::Join(cfg.f_relayUsername, nullByte, cfg.f_relayUsername, nullByte, pwPlain);

			Base64::MimeEncode(authData, authCmd, Base64::Padding::Yes, Base64::NewLines::None());
		}

		authCmd.Add("\r\n");
		SendData(ssc, SmtpSendStage::Cmd_Auth, authCmd, prevFailure);
	}


	void SmtpSenderThread::PerformAuthCramMd5(SmtpSenderCfg const& cfg, Timeouts const& timeouts, SmtpSenderConnection& ssc, Rp<SmtpSendFailure> const& prevFailure)
	{
		EnsureThrow(cfg.f_useRelay);
		EnsureThrow(cfg.f_relayAuthType != MailAuthType::None);

		ssc.m_sc->m_conn.ApplyTimeouts(timeouts, AuthReplyTimeoutMs);
		SendData(ssc, SmtpSendStage::Cmd_Auth, "AUTH CRAM-MD5\r\n", prevFailure);

		SmtpServerReply challengeReply;
		ReadReply(challengeReply, ssc, SmtpSendStage::Cmd_Auth, prevFailure);
		if (challengeReply.m_code.Value() != 334)
		{
			if (challengeReply.m_code.IsNegativePermanent())
				throw PermFailure(SmtpSendFailure_Reply(ssc, SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_UnexpectedCramMd5ChallengeReply, challengeReply, Seq(), prevFailure));
			else
				throw TempFailure(SmtpSendFailure_Reply(ssc, SmtpSendStage::Cmd_Auth, SmtpSendDetail::Auth_UnexpectedCramMd5ChallengeReply, challengeReply, Seq(), prevFailure));
		}

		Str challengeData;
		if (challengeReply.m_lines.Any())
		{
			Seq reader = challengeReply.m_lines.First();
			Base64::MimeDecode(reader, challengeData);
		}

		Str response;

		{
			BCrypt::Provider const& md5prov = m_workPool->GetMd5Provider();

			Str pwPlain;
			if (cfg.f_relayPassword.Any())
				pwPlain = Crypt::UnprotectData(cfg.f_relayPassword, CRYPTPROTECT_UI_FORBIDDEN);

			Str responseData = Str::Join(cfg.f_relayUsername, " ");

			BCrypt::Hash md5;
			md5.Init(md5prov, pwPlain);
			md5.Begin(md5prov).Update(challengeData).Final(responseData);

			Base64::MimeEncode(responseData, response, Base64::Padding::No, Base64::NewLines::None());
		}

		response.Add("\r\n");
		SendData(ssc, SmtpSendStage::Cmd_Auth, response, prevFailure);
	}

}
