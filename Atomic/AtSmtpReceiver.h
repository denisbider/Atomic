#pragma once

#include "AtEmailEntities.h"
#include "AtEmailServer.h"
#include "AtSchannel.h"
#include "AtSocket.h"
#include "AtThread.h"
#include "AtVec.h"
#include "AtWorkPool.h"


namespace At
{

	enum class EhloHostType { Unexpected, Empty, Invalid, Domain, AddrLit };

	struct EhloHost
	{
		EhloHostType m_type {};
		Str          m_host;

		EhloHost() {}
		EhloHost(EhloHostType type, Seq host) : m_type(type), m_host(host) {}
	};


	struct SmtpReceiveInstruction
	{
		bool  m_accept;
		sizet m_dataBytesToAccept;		// Use SIZE_MAX if there's no desire to further reduce the already effective max in msg size. Lowest limit takes effect
		uint  m_replyCode;
		Str   m_replyText;

		static SmtpReceiveInstruction Accept(sizet dataBytesToAccept)       { return SmtpReceiveInstruction(true, dataBytesToAccept, 0, Seq()); }
		static SmtpReceiveInstruction Refuse(uint replyCode, Seq replyText) { return SmtpReceiveInstruction(false, 0, replyCode, replyText); }
		static SmtpReceiveInstruction Refuse_Unexpected()                   { return SmtpReceiveInstruction(false, 0, 554, "5.3.0 Unexpected internal state"); }

	private:
		SmtpReceiveInstruction(bool accept, sizet dataBytesToAccept, uint replyCode, Seq replyText)
			: m_accept(accept), m_dataBytesToAccept(dataBytesToAccept), m_replyCode(replyCode), m_replyText(replyText) {}
	};



	class SmtpReceiverAuthCx : public RefCountable
	{
	public:
		// Only called on AuthCx if authentication was performed and was successful. Otherwise, SmtpReceiver::SmtpReceiver_OnMailFrom_NoAuth is called.
		// Just because this function returns SmtpReceiveInstruction::Accept doesn't mean the MAIL FROM command will be replied to positively.
		// If dataBytesToAccept is less than the declared message size, the MAIL FROM command will be refused.
		virtual SmtpReceiveInstruction SmtpReceiverAuthCx_OnMailFrom_HaveAuth(Seq fromMailbox) = 0;

		// Just because this function returns SmtpReceiveInstruction::Accept doesn't mean the RCPT TO command will be replied to positively.
		// If dataBytesToAccept is less than the declared message size, the RCPT TO command will be refused.
		virtual SmtpReceiveInstruction SmtpReceiverAuthCx_OnRcptTo(Seq toMailbox) = 0;

		// "toMailboxes" might not contain all addresses for which SmtpReceiverAuthCx_OnRcptTo returned SmtpReceiveInstruction::Accept.
		// If there were RCPT TO commands where dataBytesToAccept was less than the declared message size, those were refused.
		virtual SmtpReceiveInstruction SmtpReceiverAuthCx_OnData(Vec<Str> const& toMailboxes, Seq data) = 0;
	};



	class SmtpReceiverThread;

	class SmtpReceiver : public EmailServer<SmtpReceiverThread>
	{
	protected:
		// Called separately by main thread, as well as each receiver thread. Must copy or initialize SMTP receiver configuration into the provided entity.
		virtual void SmtpReceiver_GetCfg(SmtpReceiverCfg& cfg) const = 0;

		// Called from a variety of worker threads.
		virtual bool SmtpReceiver_TlsSupported     () { return false; }
		virtual bool SmtpReceiver_AuthSupported    () { return false; }
		virtual void SmtpReceiver_AddSchannelCerts (Schannel&);

		virtual EmailServerAuthResult SmtpReceiver_Authenticate(SockAddr const& fromHost, Schannel& conn, Seq ourName, EhloHost const& ehloHost,
			Seq authorizationIdentity, Seq authenticationIdentity, Seq password, Rp<SmtpReceiverAuthCx>& authCx);

		// Called instead of authCx.SmtpReceiverAuthCx_OnMailFrom_HaveAuth if authCx is null.
		// If successful, must set a non-null authCx representing the anonymous session.
		// The resulting authCx will have OnRcptTo and OnData methods called, but not OnMailFrom.
		// Just because this function returns SmtpReceiveInstruction::Accept doesn't mean the MAIL FROM command will be replied to positively.
		// If dataBytesToAccept is less than the declared message size, the MAIL FROM command will be refused.
		virtual SmtpReceiveInstruction SmtpReceiver_OnMailFrom_NoAuth(SockAddr const& fromHost, Schannel& conn, Seq ourName, EhloHost const& ehloHost,
			Seq fromMailbox, Rp<SmtpReceiverAuthCx>& authCx) = 0;

	private:
		SmtpReceiverCfg m_cfg { Entity::Contained };

		EntVec<EmailSrvBinding> const& EmailServer_GetCfgBindings() override final;
	
		friend class SmtpReceiverThread;
	};

}
