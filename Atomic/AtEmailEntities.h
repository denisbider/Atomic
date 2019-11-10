#pragma once

#include "AtDescEnum.h"
#include "AtDnsQuery.h"
#include "AtEntityStore.h"
#include "AtHtmlBuilder.h"
#include "AtHttpRequest.h"


namespace At
{

	DESCENUM_DECL_BEGIN(SmtpMsgStatus)
	DESCENUM_DECL_VALUE(NonFinal_Idle,    0)		// The message is enqueued, awaiting the next send attempt
	DESCENUM_DECL_VALUE(NonFinal_Sending, 1)		// There is a currently ongoing attempt to send the message
	DESCENUM_DECL_VALUE(Final_Sent,       2)		// During the last attempt, delivery succeeded for at least one mailbox. There were no pending temporary failures for other
													// mailboxes. There may have been permanent failures for some mailboxes, either in the last or a previous sending attempt.
	DESCENUM_DECL_VALUE(Final_Failed,     3)		// During the last attempt, delivery to all remaining mailboxes failed permanently.
	DESCENUM_DECL_VALUE(Final_Aborted,    4)		// After the last attempt, SmtpSender_OnDeliveryResult returned SmtpDeliveryInstr::Abort.
	DESCENUM_DECL_VALUE(Final_GivenUp,    5)		// During the last attempt, there were one or more temporary failures, but the retry schedule for the message was exhausted.
	DESCENUM_DECL_CLOSE();



	DESCENUM_DECL_BEGIN(SmtpTlsAssurance)			// Deployed in MassMailer; non-backwards-compatible changes would require conversion of existing database
	DESCENUM_DECL_VALUE(Unknown,            0)		// Unknown, e.g. because message not delivered yet
	DESCENUM_DECL_VALUE(NoTls,            100)		// Message transferred in plaintext if counterparty does not offer/start TLS, or TLS fails
	DESCENUM_DECL_VALUE(Tls_NoHostAuth,   200)		// Require TLS. No counterparty authentication
	DESCENUM_DECL_VALUE(Tls_AnyServer,    300)		// Require TLS. Counterparty authenticates, but does not have to be a host in recipient domains
	DESCENUM_DECL_VALUE(Tls_DomainMatch,  400)		// Require TLS. Counterparty MX authenticates itself as a host in recipient domains
	DESCENUM_DECL_VALUE(Tls_ExactMatch,   500)		// Require TLS. Counterparty MX authenticates itself as the exact MX name we expect
	DESCENUM_DECL_CLOSE();



	// Note: The DIGEST-MD5 mechanism, specified in RFC 2831, is moved to historical in RFC 6331 and not recommended for implementation

	DESCENUM_DECL_BEGIN(MailAuthType)
	DESCENUM_DECL_VALUE(None,               0)
	DESCENUM_DECL_VALUE(UseSuitable,      100)		// Use a suitable authentication method. PLAIN won't be used unless TLS assurance is Tls_ExactMatch
	DESCENUM_DECL_VALUE(AuthPlain,        200)		// AUTH (RFC 4954) + PLAIN (RFC 4616): Base64(authorizationId "\0" user "\0" pass)
	DESCENUM_DECL_VALUE(AuthCramMd5,      300)		// AUTH (RFC 4954) + CRAM-MD5 (RFC 2195)
	DESCENUM_DECL_CLOSE();



	DESCENUM_DECL_BEGIN(SmtpDeliveryState)			// Deployed in MassMailer; non-backwards-compatible changes would require conversion of existing database
	DESCENUM_DECL_VALUE(None,             0)
	DESCENUM_DECL_VALUE(TempFailure,      1)
	DESCENUM_DECL_VALUE(PermFailure,      2)
	DESCENUM_DECL_VALUE(Success,          3)
	DESCENUM_DECL_VALUE(GivenUp,          4)		// Not used in Atomic, but used e.g. in MassMailer to record a delivery result that won't be retried
	DESCENUM_DECL_CLOSE();



	DESCENUM_DECL_BEGIN(SmtpSendStage)
	DESCENUM_DECL_VALUE(None,               0)
	DESCENUM_DECL_VALUE(Unknown,          100)		// Used when upgrading configuration and parsing failDesc where we know there is a stage but there's no info which one
	DESCENUM_DECL_VALUE(RelayLookup,      150)
	DESCENUM_DECL_VALUE(FindMx,           200)
	DESCENUM_DECL_VALUE(Connect,          300)
	DESCENUM_DECL_VALUE(Greeting,         400)
	DESCENUM_DECL_VALUE(Cmd_Ehlo,         500)
	DESCENUM_DECL_VALUE(Capabilities,     600)
	DESCENUM_DECL_VALUE(Tls,              700)
	DESCENUM_DECL_VALUE(Cmd_Auth,         750)
	DESCENUM_DECL_VALUE(Cmd_MailFrom,     800)
	DESCENUM_DECL_VALUE(Cmd_RcptTo,       900)
	DESCENUM_DECL_VALUE(Cmd_Data,        1000)
	DESCENUM_DECL_VALUE(Content,         1100)
	DESCENUM_DECL_VALUE(Cmd_Quit,        1200)		// Not relayed outside of SmtpSender
	DESCENUM_DECL_CLOSE();



	DESCENUM_DECL_BEGIN(SmtpSendErr)
	DESCENUM_DECL_VALUE(None,                                         0)
	DESCENUM_DECL_VALUE(Unknown,                                   1000)		// Used when upgrading configuration and parsing failDesc where we can't conclusively categorize the error
	DESCENUM_DECL_VALUE(RelayLookup_LookupTimedOut,             5001000)
	DESCENUM_DECL_VALUE(RelayLookup_LookupError,                5002000)
	DESCENUM_DECL_VALUE(FindMx_LookupTimedOut,                 10001000)
	DESCENUM_DECL_VALUE(FindMx_LookupError,                    10002000)
	DESCENUM_DECL_VALUE(FindMx_LookupNoResults,                10003000)
	DESCENUM_DECL_VALUE(FindMx_DomainMatchRequired,            10004000)
	DESCENUM_DECL_VALUE(Connect_Error,                         20001000)
	DESCENUM_DECL_VALUE(Send_Error,                            30001000)
	DESCENUM_DECL_VALUE(Reply_PrematureEndOfLine,              40001000)
	DESCENUM_DECL_VALUE(Reply_UnrecognizedCodeFormat,          40002000)
	DESCENUM_DECL_VALUE(Reply_UnrecognizedLineSeparator,       40003000)
	DESCENUM_DECL_VALUE(Reply_InconsistentCode,                40004000)
	DESCENUM_DECL_VALUE(Reply_MaximumLengthExceeded,           40005000)
	DESCENUM_DECL_VALUE(Reply_ReceiveError,                    40006000)
	DESCENUM_DECL_VALUE(Greeting_SessionRefused,               50001000)
	DESCENUM_DECL_VALUE(Greeting_Unexpected,                   50002000)
	DESCENUM_DECL_VALUE(Ehlo_UnexpectedReply,                  60001000)
	DESCENUM_DECL_VALUE(Capabilities_8BitMimeRequired,         70001000)
	DESCENUM_DECL_VALUE(Capabilities_Size,                     70002000)
	DESCENUM_DECL_VALUE(Tls_NotAvailable,                      80001000)
	DESCENUM_DECL_VALUE(Tls_StartTlsRejected,                  80002000)
	DESCENUM_DECL_VALUE(Tls_SspiErr_LikelyDh_TooManyRestarts,  80003000)
	DESCENUM_DECL_VALUE(Tls_SspiErr_InvalidToken_IllegalMsg,   80004000)
	DESCENUM_DECL_VALUE(Tls_SspiErr_ServerAuthRequired,        80005000)
	DESCENUM_DECL_VALUE(Tls_SspiErr_Other,                     80006000)
	DESCENUM_DECL_VALUE(Tls_CommunicationErr,                  80007000)
	DESCENUM_DECL_VALUE(Tls_RequiredAssuranceNotAchieved,      80008000)
	DESCENUM_DECL_VALUE(Auth_AuthCommandNotSupported,          85001000)
	DESCENUM_DECL_VALUE(Auth_CfgAuthMechNotSupported,          85002000)
	DESCENUM_DECL_VALUE(Auth_NoSuitableAuthMechanism,          85003000)
	DESCENUM_DECL_VALUE(Auth_CfgAuthMechUnrecognized,          85004000)
	DESCENUM_DECL_VALUE(Auth_Rejected,                         85005000)
	DESCENUM_DECL_VALUE(Auth_UnexpectedReply,                  85006000)
	DESCENUM_DECL_VALUE(Auth_UnexpectedCramMd5ChallengeReply,  85007000)
	DESCENUM_DECL_VALUE(MailFrom_Rejected,                     90001000)
	DESCENUM_DECL_VALUE(MailFrom_UnexpectedReply,              90002000)
	DESCENUM_DECL_VALUE(RcptTo_Rejected,                      100001000)
	DESCENUM_DECL_VALUE(RcptTo_UnexpectedReply,               100002000)
	DESCENUM_DECL_VALUE(Data_Rejected,                        110001000)
	DESCENUM_DECL_VALUE(Data_UnexpectedReply,                 110002000)
	DESCENUM_DECL_VALUE(Content_Rejected,                     120001000)
	DESCENUM_DECL_VALUE(Content_UnexpectedReply,              120002000)
	DESCENUM_DECL_CLOSE()



	ENTITY_DECL_BEGIN(SmtpSenderCfg)
	ENTITY_DECL_FLD_E(IpVerPreference,	ipVerPreference)			// When making outgoing SMTP connections, whether to prefer IPv4, IPv6, or neither. Dominates destination domain MX preference
	ENTITY_DECL_FIELD(Vec<Str>,         localInterfacesIp4)			// First send attempt uses first interface; if MX disconnects, binds next one and retries. If empty, does not bind socket
	ENTITY_DECL_FIELD(Vec<Str>,         localInterfacesIp6)			// First send attempt uses first interface; if MX disconnects, binds next one and retries. If empty, does not bind socket
	ENTITY_DECL_FIELD(bool,				useRelay)					// If false, remaining fields do not matter: SmtpSender will look up destination mail exchangers and send directly
	ENTITY_DECL_FIELD(Str,				relayHost)					// DNS name, IPv4 address or IPv6 address. Should be DNS name for TLS
	ENTITY_DECL_FIELD(uint64,			relayPort)
	ENTITY_DECL_FIELD(bool,				relayImplicitTls)			// If true, assume TLS from the start of the connection. Otherwise, start plaintext SMTP and use STARTTLS later
	ENTITY_DECL_FLD_E(SmtpTlsAssurance,	relayTlsRequirement)		// TLS assurance required for connection to the relay mail exchanger, not the destination MX
	ENTITY_DECL_FLD_E(MailAuthType,		relayAuthType)				// If MailAuthType::None, remaining auth-related fields do not matter
	ENTITY_DECL_FIELD(Str,				relayUsername)
	ENTITY_DECL_FIELD(Str,				relayPassword)				// Must be decryptable using Crypt::UnprotectData
	ENTITY_DECL_CLOSE();

	enum { SmtpSenderCfg_RelayHost_MaxChars = 253 };

	HtmlBuilder& SmtpSenderCfg_RenderRows(HtmlBuilder& html, SmtpSenderCfg const& cfg);
	void SmtpSenderCfg_ReadFromPostRequest(SmtpSenderCfg& cfg, HttpRequest const& req, Vec<Str>& errs);



	class SmtpReplyCode
	{
	public:
		SmtpReplyCode() {}
		SmtpReplyCode(uint value) : m_value(value) {}
		SmtpReplyCode(uint64 value) : m_value(NumCast<uint>(value)) {}

		static SmtpReplyCode None() { return SmtpReplyCode(); }

		void EncObj(Enc& s) const { if (Any()) s.UInt(Value()); }

		bool Any                    () const noexcept { return !!m_value; }
		uint Value                  () const noexcept { return m_value; }
		bool IsPositiveCompletion   () const noexcept { return m_value >= 200 && m_value <= 299; }
		bool IsPositiveIntermediate () const noexcept { return m_value >= 300 && m_value <= 399; }
		bool IsNegativeTransient    () const noexcept { return m_value >= 400 && m_value <= 499; }
		bool IsNegativePermanent    () const noexcept { return m_value >= 500 && m_value <= 599; }

	private:
		uint m_value {};	// Value is zero if not set. A zero reply code is not valid
	};



	class SmtpEnhStatus
	{
	public:
		SmtpEnhStatus() {}

		static SmtpEnhStatus None() { return SmtpEnhStatus(); }
		static SmtpEnhStatus FromUint(uint64 value) { return SmtpEnhStatus(value); }
		static SmtpEnhStatus Read(Seq& reader);

		void EncObj(Enc& s) const;	// Encodes class.subject.detail, no whitespace before or after. Encodes nothing (empty string) if no enhanced status stored

		bool   Any        () const noexcept { return !!m_value; }
		uint64 ToUint     () const noexcept { return m_value; }
		uint   GetClass   () const noexcept { return ((m_value >> 32) & 0xFFFFU); }
		uint   GetSubject () const noexcept { return ((m_value >> 16) & 0xFFFFU); }
		uint   GetDetail  () const noexcept { return ( m_value        & 0xFFFFU); }

		// Pass UINT_MAX as cls/subject/detail to match any value in that category
		bool Match(uint cls, uint subject, uint detail) const noexcept;

	private:
		SmtpEnhStatus(uint64 value) : m_value(value) {}

		uint64 m_value {};	// Value is zero if not set. A zero value for class is not valid
	};



	class SmtpServerReply
	{
	public:
		SmtpReplyCode m_code;
		SmtpEnhStatus m_enhStatus;
		Vec<Str>      m_lines;						// Reply lines with first 4 characters (reply code and separator) removed. Enhanced status code is NOT removed

		void EncObj(Enc& s) const;
	};



	ENTITY_DECL_BEGIN(SmtpSendFailure)
	ENTITY_DECL_FLD_E(SmtpSendStage, stage)
	ENTITY_DECL_FLD_E(SmtpSendErr,   err)
	ENTITY_DECL_FIELD(Str,           mx)
	ENTITY_DECL_FIELD(uint64,        replyCode)
	ENTITY_DECL_FIELD(uint64,        enhStatus)
	ENTITY_DECL_FIELD(Str,           desc)
	ENTITY_DECL_FIELD(Vec<Str>,      lines)
	ENTITY_DECL_CLOSE();
	
	Rp<SmtpSendFailure> SmtpSendFailure_New(SmtpSendStage::E stage, SmtpSendErr::E err, LookedUpAddr const* mx,
		SmtpReplyCode code, SmtpEnhStatus enhStatus, Seq desc, Vec<Str> const* lines);

	inline Rp<SmtpSendFailure> SmtpSendFailure_RelayLookup(SmtpSendErr::E err, Seq desc)
		{ return SmtpSendFailure_New(SmtpSendStage::RelayLookup, err, nullptr, SmtpReplyCode::None(), SmtpEnhStatus::None(), desc, nullptr); }

	inline Rp<SmtpSendFailure> SmtpSendFailure_FindMx(SmtpSendErr::E err, Seq desc)
		{ return SmtpSendFailure_New(SmtpSendStage::FindMx, err, nullptr, SmtpReplyCode::None(), SmtpEnhStatus::None(), desc, nullptr); }

	inline Rp<SmtpSendFailure> SmtpSendFailure_Connect(SmtpSendErr::E err, LookedUpAddr const& mx, Seq desc)
		{ return SmtpSendFailure_New(SmtpSendStage::Connect, err, &mx, SmtpReplyCode::None(), SmtpEnhStatus::None(), desc, nullptr); }

	inline Rp<SmtpSendFailure> SmtpSendFailure_NoCode(LookedUpAddr const& mx, SmtpSendStage::E stage, SmtpSendErr::E err, Seq desc)
		{ return SmtpSendFailure_New(stage, err, &mx, SmtpReplyCode::None(), SmtpEnhStatus::None(), desc, nullptr); }

	inline Rp<SmtpSendFailure> SmtpSendFailure_Reply(LookedUpAddr const& mx, SmtpSendStage::E stage, SmtpSendErr::E err, SmtpServerReply const& reply, Seq desc)
		{ return SmtpSendFailure_New(stage, err, &mx, reply.m_code, reply.m_enhStatus, desc, &reply.m_lines); }



	ENTITY_DECL_BEGIN(MailboxResult)
	ENTITY_DECL_FIELD(Time,						time)		// This may differ even in same send attempt if some mailboxes succeed and others fail; failures before successes
	ENTITY_DECL_FIELD(Str,						mailbox)
	ENTITY_DECL_FLD_E(SmtpDeliveryState,		state)
	ENTITY_DECL_FIELD(Str,						successMx)
	ENTITY_DECL_FIELD(EntOpt<SmtpSendFailure>,	failure)
	ENTITY_DECL_CLOSE();

	inline void MailboxResult_SetSuccess(MailboxResult& x, Time t, Seq mbx, Seq successMx)
		{ x.f_time = t; x.f_mailbox = mbx; x.f_state = SmtpDeliveryState::Success;     x.f_successMx = successMx; x.f_failure.Clear(); }

	// SmtpSendFailure is moved, not copied
	inline void MailboxResult_SetTempFailure(MailboxResult& x, Time t, Seq mbx, Rp<SmtpSendFailure>&& f)
		{ x.f_time = t; x.f_mailbox = mbx; x.f_state = SmtpDeliveryState::TempFailure; x.f_successMx.Clear(); x.f_failure.Clear(); if (f.Any()) { x.f_failure.Init(std::move(f.Ref())); f.Clear(); } }
	
	// SmtpSendFailure is moved, not copied
	inline void MailboxResult_SetPermFailure(MailboxResult& x, Time t, Seq mbx, Rp<SmtpSendFailure>&& f)
		{ x.f_time = t; x.f_mailbox = mbx; x.f_state = SmtpDeliveryState::PermFailure; x.f_successMx.Clear(); x.f_failure.Clear(); if (f.Any()) { x.f_failure.Init(std::move(f.Ref())); f.Clear(); } }



	ENTITY_DECL_BEGIN(SmtpMsgToSend)
	ENTITY_DECL_FLD_K(Time,						nextAttemptTime, KeyCat::Key_NonStr_Multi)
	ENTITY_DECL_FLD_E(SmtpMsgStatus,			status)
	ENTITY_DECL_FIELD(Vec<uint64>,				futureRetryDelayMinutes)	// After each temporary failure, next value is popped and nextAttemptTime set accordingly. When exhausted, failure is permanent
	ENTITY_DECL_FLD_E(SmtpTlsAssurance,			tlsRequirement)
	ENTITY_DECL_FIELD(Vec<Str>,					additionalMatchDomains)		// If tlsRequirement == Tls_DomainMatch, additional domain names that also match; e.g. "google.com" for email sent to GMail
	ENTITY_DECL_FIELD(uint64,					baseSendSecondsMax)			// Pass zero to rely only on the extremely long timeouts defined by the SMTP RFC
	ENTITY_DECL_FIELD(uint64,					minSendBytesPerSec)			// Used if baseSendSecondsMax != 0. The smaller this number, the more the maximum send time is extended by message size
	ENTITY_DECL_FIELD(Str,						fromAddress)
	ENTITY_DECL_FIELD(Vec<Str>,					pendingMailboxes)			// Mailboxes are removed as delivery succeeds or permanently fails
	ENTITY_DECL_FIELD(Str,						toDomain)
	ENTITY_DECL_FIELD(Str,						content)
	ENTITY_DECL_FIELD(Str,						deliveryContext)			// Arbitrary binary data for use by the sending application
	ENTITY_DECL_FIELD(EntVec<MailboxResult>,	mailboxResults)
	ENTITY_DECL_CLOSE();



	// EmailSrvBinding
	// Stored as part of SmtpReceiverCfg

	enum class EmailSrvBindingProto { SMTP, POP3 };

	ENTITY_DECL_BEGIN(EmailSrvBinding)
	ENTITY_DECL_FIELD(Str,                     token)				// Unique token to be able to identify bindings across configuration changes
	ENTITY_DECL_FIELD(Str,                     intf)				// Interface - IPv4 or IPv6
	ENTITY_DECL_FIELD(uint64,                  port)				// Port number on which to listen, 1 - 65535
	ENTITY_DECL_FIELD(bool,                    ipv6Only)			// If interface is IPv6 (e.g. "::"), whether to allow only IPv6 connections, or IPv4 as well
	ENTITY_DECL_FIELD(bool,                    implicitTls)			// If true, the server will expect immediate TLS for connections through this binding
	ENTITY_DECL_FIELD(Str,                     computerName)		// For SMTP only: If non-empty, overrides computerName in SmtpReceiverCfg
	ENTITY_DECL_FIELD(uint64,                  maxInMsgKb)			// For SMTP only: If non-zero, overrides maxInMsgKb in SmtpReceiverCfg
	ENTITY_DECL_FIELD(Str,                     desc)				// Arbitrary description of the binding entry for the administrator's use
	ENTITY_DECL_CLOSE();


	inline sizet EmailSrvBindings_FindIdxByToken(EntVec<EmailSrvBinding> const& bindings, Seq token)
		{ return bindings.FindIdx( [token] (EmailSrvBinding const& x) -> bool { return x.f_token == token; } ); }

	inline EmailSrvBinding* EmailSrvBindings_FindPtrByToken(EntVec<EmailSrvBinding>& bindings, Seq token)
		{ return bindings.FindPtr( [token] (EmailSrvBinding& x) -> bool { return x.f_token == token; } ); }

	HtmlBuilder& EmailSrvBinding_RenderRows(HtmlBuilder& html, EmailSrvBinding const& binding, EmailSrvBindingProto proto);
	void EmailSrvBinding_ReadFromPostRequest(EmailSrvBinding& binding, HttpRequest const& req, EmailSrvBindingProto proto, Vec<Str>& errs);

	HtmlBuilder& EmailSrvBindings_RenderTable(HtmlBuilder& html, EntVec<EmailSrvBinding> const& bindings, EmailSrvBindingProto proto, Seq editBindingBaseUrl, Seq removeBindingBaseCmd);



	// Pop3ServerCfg

	ENTITY_DECL_BEGIN(Pop3ServerCfg)
	ENTITY_DECL_FIELD(EntVec<EmailSrvBinding>, bindings)			// Interfaces and ports on which to accept POP3 connections
	ENTITY_DECL_CLOSE()

	void Pop3ServerCfg_SetDefaultBindings(Pop3ServerCfg& cfg);



	// SmtpReceiverCfg

	ENTITY_DECL_BEGIN(SmtpReceiverCfg)
	ENTITY_DECL_FIELD(EntVec<EmailSrvBinding>, bindings)			// Interfaces and ports on which to accept SMTP connections
	ENTITY_DECL_FIELD(Str,                     computerName)		// A fully-qualified receiver computer name to send as part of the SMTP greeting. Can override in a binding
	ENTITY_DECL_FIELD(uint64,                  maxInMsgKb)			// The absolute maximum message size permitted. Can restrict further in SmtpReceiver_OnMailFrom, SmtpReceiver_OnRcptTo
	ENTITY_DECL_CLOSE()

	void SmtpReceiverCfg_SetDefaultBindings(SmtpReceiverCfg& cfg);

	HtmlBuilder& SmtpReceiverCfg_RenderRows(HtmlBuilder& html, SmtpReceiverCfg const& cfg);
	void SmtpReceiverCfg_ReadFromPostRequest(SmtpReceiverCfg& cfg, HttpRequest const& req, Vec<Str>& errs);

}
