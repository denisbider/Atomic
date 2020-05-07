#include "AtIncludes.h"
#include "AtEmailEntities.h"

#include "AtScripts.h"


namespace At
{

	DESCENUM_DEF_BEGIN(SmtpMsgStatus)
	DESCENUM_DEF_VALUE(NonFinal_Idle)
	DESCENUM_DEF_VALUE(NonFinal_Sending)
	DESCENUM_DEF_VALUE(Final_Sent)
	DESCENUM_DEF_VALUE(Final_Failed)
	DESCENUM_DEF_VALUE(Final_Aborted)
	DESCENUM_DEF_VALUE(Final_GivenUp)
	DESCENUM_DEF_CLOSE();



	DESCENUM_DEF_BEGIN(SmtpTlsAssurance)
	DESCENUM_DEF_VALUE(Unknown)
	DESCENUM_DEF_VAL_X(NoTls,            "No TLS")
	DESCENUM_DEF_VAL_X(Tls_NoHostAuth,   "TLS, host not authenticated")
	DESCENUM_DEF_VAL_X(Tls_AnyServer,    "TLS, host authenticated, host name does not match")
	DESCENUM_DEF_VAL_X(Tls_DomainMatch,  "TLS, host authenticated, host is in expected domain")
	DESCENUM_DEF_VAL_X(Tls_ExactMatch,   "TLS, host authenticated, exact host name match")
	DESCENUM_DEF_CLOSE();

	

	DESCENUM_DEF_BEGIN(MailAuthType)
	DESCENUM_DEF_VALUE(None)
	DESCENUM_DEF_VAL_X(UseSuitable,      "Use a suitable method")
	DESCENUM_DEF_VAL_X(AuthPlain,        "AUTH (RFC 4954) + PLAIN (RFC 4616)")
	DESCENUM_DEF_VAL_X(AuthCramMd5,      "AUTH (RFC 4954) + CRAM-MD5 (RFC 2195)")
	DESCENUM_DEF_CLOSE();



	DESCENUM_DEF_BEGIN(SmtpDeliveryState)
	DESCENUM_DEF_VALUE(None)
	DESCENUM_DEF_VALUE(TempFailure)
	DESCENUM_DEF_VALUE(PermFailure)
	DESCENUM_DEF_VALUE(Success)
	DESCENUM_DEF_VALUE(GivenUp)
	DESCENUM_DEF_CLOSE();



	DESCENUM_DEF_BEGIN(SmtpSendStage)
	DESCENUM_DEF_VALUE(None)
	DESCENUM_DEF_VALUE(FindMx)
	DESCENUM_DEF_VALUE(Connect)
	DESCENUM_DEF_VALUE(Greeting)
	DESCENUM_DEF_VALUE(Cmd_Ehlo)
	DESCENUM_DEF_VALUE(Capabilities)
	DESCENUM_DEF_VALUE(Tls)
	DESCENUM_DEF_VALUE(Cmd_Auth)
	DESCENUM_DEF_VALUE(Cmd_MailFrom)
	DESCENUM_DEF_VALUE(Cmd_RcptTo)
	DESCENUM_DEF_VALUE(Cmd_Data)
	DESCENUM_DEF_VALUE(Content)
	DESCENUM_DEF_VALUE(Cmd_Quit)
	DESCENUM_DEF_CLOSE();



	DESCENUM_DEF_BEGIN(SmtpSendErr)
	DESCENUM_DEF_VALUE(None)
	DESCENUM_DEF_VALUE(Unknown)
	DESCENUM_DEF_VAL_X(FindMx_LookupTimedOut,                 "Timed out while looking up destination domain MX records")
	DESCENUM_DEF_VAL_X(FindMx_LookupError,                    "Error looking up destination domain MX records")
	DESCENUM_DEF_VAL_X(FindMx_LookupNoResults,                "An attempt to look up destination domain MX records did not return any results")
	DESCENUM_DEF_VAL_X(FindMx_DomainMatchRequired,            "The message requires TLS with domain match, but DNS lookup returned no domain-matching destination mail exchangers")
	DESCENUM_DEF_VAL_X(Connect_Error,                         "Error connecting to the destination mail exchanger")
	DESCENUM_DEF_VAL_X(Send_Error,                            "Error sending bytes to the destination mail exchanger")
	DESCENUM_DEF_VAL_X(Reply_PrematureEndOfLine,              "The destination mail exchanger sent a reply line that ended prematurely")
	DESCENUM_DEF_VAL_X(Reply_UnrecognizedCodeFormat,          "The destination mail exchanger sent a reply line with an unrecognized code format")
	DESCENUM_DEF_VAL_X(Reply_UnrecognizedLineSeparator,       "The destination mail exchanger sent reply lines with an unrecognized line separator")
	DESCENUM_DEF_VAL_X(Reply_InconsistentCode,                "The destination mail exchanger sent reply lines with inconsistent reply codes")
	DESCENUM_DEF_VAL_X(Reply_MaximumLengthExceeded,           "The destination mail exchanger sent a reply that exceeds maximum length")
	DESCENUM_DEF_VAL_X(Reply_ReceiveError,                    "Error receiving reply from the destination mail exchanger")
	DESCENUM_DEF_VAL_X(Greeting_SessionRefused,               "The destination mail exchanger refused the session in its SMTP greeting")
	DESCENUM_DEF_VAL_X(Greeting_Unexpected,                   "The destination mail exchanger sent an unexpected greeting")
	DESCENUM_DEF_VAL_X(Ehlo_UnexpectedReply,                  "The destination mail exchanger sent an unexpected EHLO reply code")
	DESCENUM_DEF_VAL_X(Capabilities_8BitMimeRequired,         "The message requires 8BITMIME, but the destination mail exchanger does not support it")
	DESCENUM_DEF_VAL_X(Capabilities_Size,                     "The message exceeds the destination mail exchanger's size limit")
	DESCENUM_DEF_VAL_X(Tls_NotAvailable,                      "The message requires TLS, but the destination mail exchanger does not appear to support it")
	DESCENUM_DEF_VAL_X(Tls_StartTlsRejected,                  "The message requires TLS, but the destination mail exchanger rejected the STARTTLS command")
	DESCENUM_DEF_VAL_X(Tls_SspiErr_LikelyDh_TooManyRestarts,  "The message requires TLS, but TLS could not be started after connecting multiple times due to a likely DH error")
	DESCENUM_DEF_VAL_X(Tls_SspiErr_InvalidToken_IllegalMsg,   "The message requires TLS, but TLS could not be started due to SEC_E_INVALID_TOKEN or SEC_E_ILLEGAL_MESSAGE")
	DESCENUM_DEF_VAL_X(Tls_SspiErr_ServerAuthRequired,        "The message requires TLS server authentication, but the destination mail exchanger's identity could not be verified")
	DESCENUM_DEF_VAL_X(Tls_SspiErr_Other,                     "The message requires TLS, but TLS could not be started due to an SSPI error")
	DESCENUM_DEF_VAL_X(Tls_CommunicationErr,                  "The message requires TLS, but TLS could not be started due to a communication error")
	DESCENUM_DEF_VAL_X(Tls_RequiredAssuranceNotAchieved,      "The TLS assurance level required to send message was not achieved")
	DESCENUM_DEF_VAL_X(Auth_AuthCommandNotSupported,          "Authentication is configured but the mail exchanger does not support the AUTH command")
	DESCENUM_DEF_VAL_X(Auth_CfgAuthMechNotSupported,          "Authentication is configured but the mail exchanger does not support the configured authentication mechanism")
	DESCENUM_DEF_VAL_X(Auth_NoSuitableAuthMechanism,          "Authentication is configured but the mail exchanger does not support a suitable authentication mechanism")
	DESCENUM_DEF_VAL_X(Auth_CfgAuthMechUnrecognized,          "The configured SMTP relay authentication type is unrecognized")
	DESCENUM_DEF_VAL_X(Auth_Rejected,                         "The mail exchanger rejected our AUTH command")
	DESCENUM_DEF_VAL_X(Auth_UnexpectedReply,                  "The mail exchanger sent an unexpected response to our AUTH command")
	DESCENUM_DEF_VAL_X(Auth_UnexpectedCramMd5ChallengeReply,  "The mail exchanger sent an unexpected challenge response to our AUTH CRAM-MD5 command")
	DESCENUM_DEF_VAL_X(MailFrom_Rejected,                     "The destination mail exchanger rejected our MAIL FROM command")
	DESCENUM_DEF_VAL_X(MailFrom_UnexpectedReply,              "The destination mail exchanger sent an unexpected response to our MAIL FROM command")
	DESCENUM_DEF_VAL_X(RcptTo_Rejected,                       "The destination mail exchanger rejected our RCPT TO command")
	DESCENUM_DEF_VAL_X(RcptTo_UnexpectedReply,                "The destination mail exchanger sent an unexpected response to our RCPT TO command")
	DESCENUM_DEF_VAL_X(Data_Rejected,                         "The destination mail exchanger rejected our DATA command")
	DESCENUM_DEF_VAL_X(Data_UnexpectedReply,                  "The destination mail exchanger sent an unexpected response to our DATA command")
	DESCENUM_DEF_VAL_X(Content_Rejected,                      "The destination mail exchanger rejected our message content")
	DESCENUM_DEF_VAL_X(Content_UnexpectedReply,               "The destination mail exchanger sent an unexpected response to our message content")
	DESCENUM_DEF_CLOSE()



	ENTITY_DEF_BEGIN(SmtpSenderCfg)
	ENTITY_DEF_FLD_V(SmtpSenderCfg, memUsageLimitBytes, 2)
	ENTITY_DEF_F_E_V(SmtpSenderCfg, ipVerPreference,    1)
	ENTITY_DEF_FLD_V(SmtpSenderCfg, localInterfacesIp4, 1)
	ENTITY_DEF_FLD_V(SmtpSenderCfg, localInterfacesIp6, 1)
	ENTITY_DEF_FIELD(SmtpSenderCfg, useRelay)
	ENTITY_DEF_FIELD(SmtpSenderCfg, relayHost)
	ENTITY_DEF_FIELD(SmtpSenderCfg, relayPort)
	ENTITY_DEF_FIELD(SmtpSenderCfg, relayImplicitTls)
	ENTITY_DEF_FLD_E(SmtpSenderCfg, relayTlsRequirement)
	ENTITY_DEF_FLD_E(SmtpSenderCfg, relayAuthType)
	ENTITY_DEF_FIELD(SmtpSenderCfg, relayUsername)
	ENTITY_DEF_FIELD(SmtpSenderCfg, relayPassword)
	ENTITY_DEF_CLOSE(SmtpSenderCfg);


	namespace { char const* c_zSmtpRelayPwValueNoChange = "xxxxxxxxxxxxxxxxxxxx"; }
	
	HtmlBuilder& SmtpSenderCfg_RenderRows(HtmlBuilder& html, SmtpSenderCfg const& cfg)
	{
		html.Tr()
				.Td().P().Label("smtpSender_memUsageLimitBytes", "SMTP sender memory usage limit bytes").EndP().EndTd()
				.Td()
					.P().InputNumber().IdAndName("smtpSender_memUsageLimitBytes").Value(Str().UInt(cfg.f_memUsageLimitBytes)).EndP()
					.P().Class("help").T("Message content is kept in memory when sending. If non-zero, the SMTP sender will avoid enqueueing messages "
						"when in-memory content is above this size.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_ipVerPreference", "SMTP sender IPv4/6 preference").EndP().EndTd()
				.Td()
					.P().Select().IdAndName("smtpSender_ipVerPreference").Fun(IpVerPreference::RenderOptions, cfg.f_ipVerPreference).EndSelect().EndP()
					.P().Class("help").T("When making outgoing SMTP connections, whether to prefer IPv4, IPv6, or neither. "
						"If a preference is configured, it dominates MX preferences configured in DNS for destination domains.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_localInterfacesIp4", "Local interfaces - IPv4").EndP().EndTd()
				.Td()
					.TextArea().IdAndName("smtpSender_localInterfacesIp4").Rows("5").Cols("25")
						.T(Str().AddN(cfg.f_localInterfacesIp4, "\r\n"))
					.EndTextArea()
					.P().Class("help").T("Enter one IPv4 address per line. When connecting to a mail exchanger, the first attempt will bind the first interface configured. "
						"If the mail exchanger refuses the connection in its SMTP greeting, another attempt will be made with the second interface, and so forth.").EndP()
					.P().Class("help").T("If not configured, local interfaces for outgoing connections will be left for the OS to choose.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_localInterfacesIp6", "Local interfaces - IPv6").EndP().EndTd()
				.Td()
					.TextArea().IdAndName("smtpSender_localInterfacesIp6").Rows("5").Cols("50")
						.T(Str().AddN(cfg.f_localInterfacesIp6, "\r\n"))
					.EndTextArea()
					.P().Class("help").T("Enter one IPv6 address per line. When connecting to a mail exchanger, the first attempt will bind the first interface configured. "
						"If the mail exchanger refuses the connection in its SMTP greeting, another attempt will be made with the second interface, and so forth.").EndP()
					.P().Class("help").T("If not configured, local interfaces for outgoing connections will be left for the OS to choose.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().InputCheckbox().IdAndName("smtpSender_useRelay").CheckedIf(cfg.f_useRelay).EndP().EndTd()
				.Td()
					.P().Label("smtpSenderCfg_useRelay", "Use SMTP relay").EndP()
					.P().Class("help").T("If enabled, outgoing messages will be sent through the specified SMTP server. "
						"If disabled, messages will be delivered to individual mail exchangers for each destination address.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayHost", "SMTP relay host").EndP().EndTd()
				.Td()
					.P().InputDnsName("mx.example.com").IdAndName("smtpSender_relayHost").Value(cfg.f_relayHost).EndP()
					.P().Class("help").T("The DNS name, IPv4 address, or IPv6 address of the SMTP relay. Use a ").B("DNS name").T(" for TLS security.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayPort", "SMTP relay port").EndP().EndTd()
				.Td()
					.P().InputNumber().IdAndName("smtpSender_relayPort").Value(Str().UInt(cfg.f_relayPort)).EndP()
					.P().Class("help").T("Usual SMTP port numbers are ").B("25").T(" when using STARTTLS, ").B("465").T(" when using implicit TLS.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().InputCheckbox().IdAndName("smtpSender_relayImplicitTls").CheckedIf(cfg.f_relayImplicitTls).EndP().EndTd()
				.Td()
					.P().Label("smtpSender_relayImplicitTls", "Implicit TLS").EndP()
					.P().Class("help").T("If enabled, assume TLS from the start of the connection. Otherwise, start plaintext SMTP and use STARTTLS later.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayTlsRequirement", "SMTP relay TLS requirement").EndP().EndTd()
				.Td()
					.P().Select().IdAndName("smtpSender_relayTlsRequirement").Fun(SmtpTlsAssurance::RenderOptions, cfg.f_relayTlsRequirement).EndSelect().EndP()
					.P().Class("help").T("Minimum TLS assurance that must be achieved before authenticating and/or sending messages through the relay. "
						"The host name authenticated via TLS is matched against relay host name configured in this section.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayAuthType", "SMTP relay authentication type").EndP().EndTd()
				.Td().P().Select().IdAndName("smtpSender_relayAuthType").Fun(MailAuthType::RenderOptions, cfg.f_relayAuthType).EndSelect().EndP().EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayUsername", "SMTP relay username").EndP().EndTd()
				.Td().P().InputName().IdAndName("smtpSender_relayUsername").Value(cfg.f_relayUsername).EndP().EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpSender_relayPassword", "SMTP relay password").EndP().EndTd()
				.Td().P().InputPassword(fit_pw_noMinLen).Value(cfg.f_relayPassword.Any() ? c_zSmtpRelayPwValueNoChange : "").EndP().EndTd()
			.EndTr();

		html.AddJs(c_js_AtSmtpEntities_SenderCfg);

		return html;
	}


	void SmtpSenderCfg_ReadFromPostRequest(SmtpSenderCfg& cfg, HttpRequest const& req, Vec<Str>& errs)
	{
		cfg.f_memUsageLimitBytes   =  req.PostNvp("smtpSender_memUsageLimitBytes" ).Trim().ReadNrUInt64Dec();
		Seq ipVerPreferenceStr     =  req.PostNvp("smtpSender_ipVerPreference"    );
		Seq localInterfacesIp4Str  =  req.PostNvp("smtpSender_localInterfacesIp4" );
		Seq localInterfacesIp6Str  =  req.PostNvp("smtpSender_localInterfacesIp6" );
		cfg.f_useRelay             = (req.PostNvp("smtpSender_useRelay"           ) == "1");
		cfg.f_relayHost            =  req.PostNvp("smtpSender_relayHost"          ).Trim().ReadUtf8_MaxChars(SmtpSenderCfg_RelayHost_MaxChars);
		uint64 relayPort           =  req.PostNvp("smtpSender_relayPort"          ).Trim().ReadNrUInt64Dec();
		cfg.f_relayImplicitTls     = (req.PostNvp("smtpSender_relayImplicitTls"   ) == "1");
		Seq relayTlsRequirementStr =  req.PostNvp("smtpSender_relayTlsRequirement");
		Seq relayAuthTypeStr       =  req.PostNvp("smtpSender_relayAuthType"      );
		cfg.f_relayUsername        =  req.PostNvp("smtpSender_relayUsername"      ).ReadUtf8_MaxChars(fit_name.mc_nMaxLen).Trim();
		Seq relayPassword          =  req.PostNvp("smtpSender_relayPassword"      ).ReadUtf8_MaxChars(fit_pw_noMinLen.mc_nMaxLen).Trim();

		IpVerPreference::E ipVerPreference;
		if (!IpVerPreference::ReadNrAndVerify(ipVerPreferenceStr, ipVerPreference))
			errs.Add("Invalid IP version preference");
		else
			cfg.f_ipVerPreference = ipVerPreference;

		cfg.f_localInterfacesIp4 = localInterfacesIp4Str.SplitLines<Str>(SplitFlags::Trim | SplitFlags::DiscardEmpty);
		cfg.f_localInterfacesIp6 = localInterfacesIp6Str.SplitLines<Str>(SplitFlags::Trim | SplitFlags::DiscardEmpty);

		if (relayPort < 1 || relayPort > 65535)
			errs.Add("Invalid relay port number");
		else
			cfg.f_relayPort = relayPort;

		SmtpTlsAssurance::E relayTlsRequirement;
		if (!SmtpTlsAssurance::ReadNrAndVerify(relayTlsRequirementStr, relayTlsRequirement))
			errs.Add("Invalid relay TLS requirement");
		else
			cfg.f_relayTlsRequirement = relayTlsRequirement;

		MailAuthType::E relayAuthType;
		if (!MailAuthType::ReadNrAndVerify(relayAuthTypeStr, relayAuthType))
			errs.Add("Invalid relay authentication type");
		else
			cfg.f_relayAuthType = relayAuthType;

		cfg.f_relayPassword = Crypt::ProtectData(relayPassword, CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN);
	}
	


	ENTITY_DEF_BEGIN(SmtpSendFailure)
	ENTITY_DEF_FLD_E(SmtpSendFailure, stage)
	ENTITY_DEF_FIELD(SmtpSendFailure, mx)
	ENTITY_DEF_FIELD(SmtpSendFailure, replyCode)
	ENTITY_DEF_FIELD(SmtpSendFailure, enhStatus)
	ENTITY_DEF_FIELD(SmtpSendFailure, desc)
	ENTITY_DEF_FIELD(SmtpSendFailure, lines)
	ENTITY_DEF_CLOSE(SmtpSendFailure);


	Rp<SmtpSendFailure> SmtpSendFailure_New(SmtpSendStage::E stage, SmtpSendErr::E err, LookedUpAddr const* mx,
		SmtpReplyCode code, SmtpEnhStatus enhStatus, Seq desc, Vec<Str> const* lines)
	{
		Rp<SmtpSendFailure> f = new SmtpSendFailure(Entity::Contained);
		f->f_stage = stage;
		f->f_err = err;
		if (mx)
			mx->EncObj(f->f_mx);
		f->f_replyCode = code.Value();
		f->f_enhStatus = enhStatus.ToUint();
		f->f_desc      = desc;
		if (lines)
			f->f_lines = *lines;
		return f;
	}


	void SmtpSendFailure_EncodeAsSmtpDiagnosticCode(SmtpSendFailure const& f, Enc& s, SmtpDeliveryState::E deliveryState)
	{
		bool needNewLine {};

		auto addLine = [&needNewLine, &s] (Seq line)
			{
				line = line.Trim();
				if (line.n)
				{
					if (needNewLine) s.Add("\r\n "); else needNewLine = true;

					while (line.n)
					{
						uint c;
						if (Utf8::ReadResult::OK != Utf8::ReadCodePoint(line, c))	{ s.Add("&#65533;"); line.DropByte(); }		// Unicode replacement character
						else if (c == '&')											s.Add("&amp;");
						else if (c == '(')											s.Add("&lpar;");
						else if (c == ')')											s.Add("&rpar;");
						else if (c < 32 || c > 126)									s.Add("&#").UInt(c).Ch(';');
					}
				}
			};

		auto beginComment = [&needNewLine, &s] ()
			{
				if (needNewLine) s.Add("\r\n "); else needNewLine = true;
				s.Ch('(');
			};

		auto endComment = [&s] () { s.Ch(')'); };

		if (!f.f_lines.Any())
		{
			if (SmtpDeliveryState::TempFailure == deliveryState)
				addLine("420 Temporary delivery failure");
			else
				addLine("520 Permanent delivery failure");
		}
		else
		{
			Str linePrefixed;
			for (sizet i=0; i!=f.f_lines.Len(); ++i)
			{
				Seq line = f.f_lines[i];
				char sep = (i + 1 < f.f_lines.Len()) ? '-' : ' ';
				linePrefixed.Clear().ReserveAtLeast(4 + line.n);
				linePrefixed.UInt(f.f_replyCode).Ch(sep).Add(line);
				addLine(linePrefixed);
			}
		}

		beginComment();

		Str line;
		line.SetAdd("Send stage ", SmtpSendStage::Name(f.f_stage), ", ");

		Seq errName = SmtpSendErr::Name(f.f_err);
		Seq errDesc = SmtpSendErr::Desc(f.f_err);
		if (errName.EqualInsensitive(errDesc))
		{
			line.Add("error ").Add(errName);
			if (f.f_desc.Any())
				line.Ch(';');
			addLine(line);
		}
		else
		{
			line.Add("error ").Add(errName).Add(":");
			addLine(line);

			line.Set(errDesc);
			if (f.f_desc.Any())
				line.Ch(';');
			addLine(line);
		}

		Seq descReader = f.f_desc;
		while (descReader.n)
		{
			Seq descLine = descReader.ReadToFirstByteOf("\r\n");
			descReader.DropToFirstByteNotOf("\r\n\t ");
			addLine(descLine);
		}

		endComment();
	}



	ENTITY_DEF_BEGIN(MailboxResult)
	ENTITY_DEF_FIELD(MailboxResult, time)
	ENTITY_DEF_FIELD(MailboxResult, mailbox)
	ENTITY_DEF_FLD_E(MailboxResult, state)
	ENTITY_DEF_FIELD(MailboxResult, successMx)
	ENTITY_DEF_FIELD(MailboxResult, failure)
	ENTITY_DEF_CLOSE(MailboxResult);

	
	void MailboxResultCount::Count(Slice<MailboxResult> results)
	{
		for (MailboxResult const& r : results)
			switch (r.f_state)
			{
			case SmtpDeliveryState::Success: ++m_nrSuccess; break;
			case SmtpDeliveryState::TempFailure: ++m_nrTempFail; break;
			case SmtpDeliveryState::PermFailure: ++m_nrPermFail; break;
			default: EnsureThrow(!"Unexpected SmtpDeliveryState value");
			}
	}



	ENTITY_DEF_BEGIN(SmtpMsgToSend)
	ENTITY_DEF_FIELD(SmtpMsgToSend, nextAttemptTime)
	ENTITY_DEF_FLD_E(SmtpMsgToSend, status)
	ENTITY_DEF_FIELD(SmtpMsgToSend, futureRetryDelayMinutes)
	ENTITY_DEF_FLD_E(SmtpMsgToSend, tlsRequirement)
	ENTITY_DEF_FIELD(SmtpMsgToSend, additionalMatchDomains)
	ENTITY_DEF_FIELD(SmtpMsgToSend, baseSendSecondsMax)
	ENTITY_DEF_FIELD(SmtpMsgToSend, nrBytesToAddOneSec)
	ENTITY_DEF_FIELD(SmtpMsgToSend, fromAddress)
	ENTITY_DEF_FIELD(SmtpMsgToSend, pendingMailboxes)
	ENTITY_DEF_FIELD(SmtpMsgToSend, toDomain)
	ENTITY_DEF_FIELD(SmtpMsgToSend, contentPart1)
	ENTITY_DEF_FLD_V(SmtpMsgToSend, moreContentContext, 1)
	ENTITY_DEF_FIELD(SmtpMsgToSend, deliveryContext)
	ENTITY_DEF_FIELD(SmtpMsgToSend, mailboxResults)
	ENTITY_DEF_CLOSE(SmtpMsgToSend);



	void SmtpServerReply::EncObj(Enc& s) const
	{
		s.Add("Reply code ").UInt(m_code.Value());
		if (!m_lines.Any())
			s.Add(", no content");
		else if (m_lines.Len() == 1)
			s.Add(", line: ").Add(m_lines[0]);
		else
		{
			s.Add(", content:\r\n").Add(m_lines[0]).Add("\r\n");
			for (Str const& line : m_lines.GetSlice(1))
				s.Add(line).Add("\r\n");
		}
	}



	// EmailSrvBinding

	ENTITY_DEF_BEGIN(EmailSrvBinding)
	ENTITY_DEF_FIELD(EmailSrvBinding, token)
	ENTITY_DEF_FIELD(EmailSrvBinding, intf)
	ENTITY_DEF_FIELD(EmailSrvBinding, port)
	ENTITY_DEF_FIELD(EmailSrvBinding, ipv6Only)
	ENTITY_DEF_FIELD(EmailSrvBinding, implicitTls)
	ENTITY_DEF_FIELD(EmailSrvBinding, computerName)
	ENTITY_DEF_FIELD(EmailSrvBinding, maxInMsgKb)
	ENTITY_DEF_FIELD(EmailSrvBinding, desc)
	ENTITY_DEF_CLOSE(EmailSrvBinding);


	HtmlBuilder& EmailSrvBinding_RenderRows(HtmlBuilder& html, EmailSrvBinding const& binding, EmailSrvBindingProto proto)
	{
		html.Tr()
				.Td().P().Label("emailSrvBinding_intf", "Interface").EndP().EndTd()
				.Td()
					.P().InputIp4Or6().IdAndName("emailSrvBinding_intf").Value(binding.f_intf).EndP()
					.P().Class("help").T("The IPv4 or IPv6 address of the interface on which to accept connections.").EndP()
					.P().Class("help").T("Use ").B("0.0.0.0").T(" (IPv4) or ").B("::").T(" (IPv6) to accept connections arriving on any interface. "
						"Use ").B("127.0.0.1").T(" (IPv4) or ").B("::1").T(" (IPv6) to accept only connections from the same computer.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("emailSrvBinding_port", "Port").EndP().EndTd()
				.Td()
					.P().InputNumber().IdAndName("emailSrvBinding_port").MinAttr("1").MaxAttr("65535").Value(Str().UInt(binding.f_port)).EndP()
					.P().Class("help").T("The port number on which to accept connections.").EndP();

		if (EmailSrvBindingProto::SMTP == proto)
			html	.P().Class("help").T("Default port numbers: ").B("25").T(" - plain SMTP (explicit TLS); ").B("465").T(" - SMTP over implicit TLS.").EndP();
		else if (EmailSrvBindingProto::POP3 == proto)
			html	.P().Class("help").T("Default port numbers: ").B("110").T(" - plain POP3 (explicit TLS), ").B("995").T(" - POP3 over implicit TLS.").EndP();
		else
			EnsureThrow(!"Unrecognized protocol");

		html	.EndTd()
			.EndTr()
			.Tr()
				.Td().P().InputCheckbox().IdAndName("emailSrvBinding_ipv6Only").CheckedIf(binding.f_ipv6Only).EndP().EndTd()
				.Td()
					.P().Label("emailSrvBinding_ipv6Only", "IPv6 only").EndP()
					.P().Class("help").T("If the interface is an IPv6 address such as ").B("::").T(", then if this is checked, only IPv6 connections will be accepted. "
						"If not checked, both IPv6 and IPv4 connections will be accepted if the OS permits. Has no effect if the interface is an IPv4 address.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().InputCheckbox().IdAndName("emailSrvBinding_implicitTls").CheckedIf(binding.f_implicitTls).EndP().EndTd()
				.Td()
					.P().Label("emailSrvBinding_implicitTls", "Implicit TLS").EndP()
					.P().Class("help").T("If checked, connections to this binding must start TLS immediately. If not checked, the connection starts in plaintext "
						"and the client can start TLS using the ").B(proto == EmailSrvBindingProto::POP3 ? "STLS" : "STARTTLS").T(" command.").EndP()
				.EndTd()
			.EndTr();

		if (EmailSrvBindingProto::SMTP == proto)
		{
			html.Tr()
					.Td().P().Label("emailSrvBinding_computerName", "Computer name").EndP().EndTd()
					.Td()
						.P().InputDnsName().IdAndName("emailSrvBinding_computerName").Value(binding.f_computerName).EndP()
						.P().Class("help").T("A fully-qualified DNS name of this computer that should be sent to connections at this binding. "
							"If not configured (empty), the global SMTP receiver setting is used.").EndP()
					.EndTd()
				.EndTr()
				.Tr()
					.Td().P().Label("emailSrvBinding_maxInMsgKb", "Maximum incoming message size (kB)").EndP().EndTd()
					.Td()
						.P().InputNumber().IdAndName("emailSrvBinding_maxInMsgKb").Value(Str().UInt(binding.f_maxInMsgKb)).EndP()
						.P().Class("help").T("The maximum incoming message size that will be accepted. "
							"If not configured (set to zero), the global SMTP receiver setting is used.").EndP()
					.EndTd()
				.EndTr();
		}

		html.Tr()
				.Td().P().Label("emailSrvBinding_desc", "Description").EndP().EndTd()
				.Td()
					.P().InputDesc().IdAndName("emailSrvBinding_desc").Value(binding.f_desc).EndP()
					.P().Class("help").T("Arbitrary text to describe this binding.").EndP()
				.EndTd()
			.EndTr();

		return html;
	}

	
	void EmailSrvBinding_ReadFromPostRequest(EmailSrvBinding& binding, HttpRequest const& req, EmailSrvBindingProto proto, Vec<Str>& errs)
	{
		Seq intf               =  req.PostNvp("emailSrvBinding_intf"         ).Trim();
		uint64 port            =  req.PostNvp("emailSrvBinding_port"         ).Trim().ReadNrUInt64Dec();
		binding.f_ipv6Only     = (req.PostNvp("emailSrvBinding_ipv6Only"     ) == "1");
		binding.f_implicitTls  = (req.PostNvp("emailSrvBinding_implicitTls"  ) == "1");
		binding.f_desc         =  req.PostNvp("emailSrvBinding_desc"         );

		SockAddr sa;
		if (!sa.Parse(intf).IsIp4Or6())
			errs.Add("Invalid interface");
		else
			binding.f_intf = intf;

		if (port < 1 || port > 65535)
			errs.Add("Invalid port number");
		else
			binding.f_port = port;

		if (EmailSrvBindingProto::SMTP == proto)
		{
			binding.f_computerName = req.PostNvp("emailSrvBinding_computerName" ).Trim();
			binding.f_maxInMsgKb   = req.PostNvp("emailSrvBinding_maxInMsgKb"   ).Trim().ReadNrUInt64Dec();
		}
	}


	HtmlBuilder& EmailSrvBindings_RenderTable(HtmlBuilder& html, EntVec<EmailSrvBinding> const& bindings, EmailSrvBindingProto proto, Seq editBindingBaseUrl, Seq removeBindingBaseCmd)
	{
		html.Table()
				.Tr()
					.Th("Interface")
					.Th("Port")
					.Th("IPv6 only")
					.Th("Implicit TLS");

		if (EmailSrvBindingProto::SMTP == proto)
		{
			html	.Th("Computer")
					.Th("Max msg size");
		}

		html		.Th("Description")
					.Th("Edit")
					.Th("Remove")
				.EndTr();

		for (EmailSrvBinding const& b : bindings)
		{
			html.Tr()
					.Td(b.f_intf)
					.Td(Str().UInt(b.f_port))
					.Td(b.f_ipv6Only    ? "&check;" : "")
					.Td(b.f_implicitTls ? "&check;" : "");

			if (EmailSrvBindingProto::SMTP == proto)
			{
				html.Td(b.f_computerName)
					.Td(b.f_maxInMsgKb ? Str().UInt(b.f_maxInMsgKb) : Str());
			}

			html	.Td(b.f_desc)
					.Td().A(Str::Join(editBindingBaseUrl, b.f_token), "Edit").EndTd()
					.Td().ConfirmSubmit("Confirm", Str::Join(removeBindingBaseCmd, b.f_token), "Remove").EndTd()
				.EndTr();
		}

		html.EndTable();
		return html;
	}



	// Pop3ServerCfg

	ENTITY_DEF_BEGIN(Pop3ServerCfg)
	ENTITY_DEF_FIELD(Pop3ServerCfg, bindings)
	ENTITY_DEF_CLOSE(Pop3ServerCfg);

	
	void Pop3ServerCfg_SetDefaultBindings(Pop3ServerCfg& cfg)
	{
		cfg.f_bindings.Clear();
		
		{	EmailSrvBinding& b = cfg.f_bindings.Add();
			b.f_token = Token::Generate();
			b.f_intf = "::";
			b.f_port = 110; }

		{	EmailSrvBinding& b = cfg.f_bindings.Add();
			b.f_token = Token::Generate();
			b.f_intf = "::";
			b.f_port = 995;
			b.f_implicitTls = true; }
	}



	// SmtpReceiverCfg

	ENTITY_DEF_BEGIN(SmtpReceiverCfg)
	ENTITY_DEF_FIELD(SmtpReceiverCfg, bindings)
	ENTITY_DEF_FIELD(SmtpReceiverCfg, computerName)
	ENTITY_DEF_FIELD(SmtpReceiverCfg, maxInMsgKb)
	ENTITY_DEF_CLOSE(SmtpReceiverCfg);

	
	void SmtpReceiverCfg_SetDefaultBindings(SmtpReceiverCfg& cfg)
	{
		cfg.f_bindings.Clear();
		
		{	EmailSrvBinding& b = cfg.f_bindings.Add();
			b.f_token = Token::Generate();
			b.f_intf = "::";
			b.f_port = 25; }

		{	EmailSrvBinding& b = cfg.f_bindings.Add();
			b.f_token = Token::Generate();
			b.f_intf = "::";
			b.f_port = 465;
			b.f_implicitTls = true; }
	}


	HtmlBuilder& SmtpReceiverCfg_RenderRows(HtmlBuilder& html, SmtpReceiverCfg const& cfg)
	{
		html.Tr()
				.Td().P().Label("smtpReceiverCfg_computerName", "Computer name").EndP().EndTd()
				.Td()
					.P().InputDnsName().IdAndName("smtpReceiverCfg_computerName").Value(cfg.f_computerName).EndP()
					.P().Class("help").T("A fully-qualified DNS name of this computer that should be sent to SMTP senders. "
						"Can be overridden in settings for an individual binding.").EndP()
				.EndTd()
			.EndTr()
			.Tr()
				.Td().P().Label("smtpReceiverCfg_maxInMsgKb", "Maximum incoming message size (kB)").EndP().EndTd()
				.Td()
					.P().InputNumber().IdAndName("smtpReceiverCfg_maxInMsgKb").Value(Str().UInt(cfg.f_maxInMsgKb)).EndP()
					.P().Class("help").T("The maximum incoming message size that will be accepted. "
						"Can be overridden in settings for an individual binding.").EndP()
				.EndTd()
			.EndTr();

		return html;
	}


	void SmtpReceiverCfg_ReadFromPostRequest(SmtpReceiverCfg& cfg, HttpRequest const& req, Vec<Str>& errs)
	{
		cfg.f_computerName = req.PostNvp("smtpReceiverCfg_computerName" ).Trim();
		uint64 maxInMsgKb  = req.PostNvp("smtpReceiverCfg_maxInMsgKb"   ).Trim().ReadNrUInt64Dec();

		if (!maxInMsgKb)
			errs.Add("Invalid maximum incoming message size");
		else
			cfg.f_maxInMsgKb = maxInMsgKb;
	}

}
