#include "AutIncludes.h"



// AutSmtpReceiver

class AutSmtpReceiver : public SmtpReceiver
{
public:

public:
	void WorkPool_LogEvent(WORD eventType, Seq text);

	enum { MaxInMsgKb = 10*1000, MaxInMsgBytes = MaxInMsgKb * 1024 };
	void SmtpReceiver_GetCfg(SmtpReceiverCfg& cfg) const override final;

	EmailServerAuthResult SmtpReceiver_Authenticate(SockAddr const&, Schannel&, Seq, EhloHost const&, Seq, Seq, Seq, Rp<SmtpReceiverAuthCx>&) override final
		{ return EmailServerAuthResult::InvalidCredentials; }

	SmtpReceiveInstruction SmtpReceiver_OnMailFrom_NoAuth(SockAddr const& fromHost, Schannel& conn, Seq ourName, EhloHost const& ehloHost,
			Seq fromMailbox, Rp<SmtpReceiverAuthCx>& authCx) override final;

	static SmtpReceiveInstruction RandomReply(std::function<void(char const*)> action);

private:
	struct AuthCx : SmtpReceiverAuthCx
	{
		SockAddr m_fromHost;
		Str m_fromMailbox;

		AuthCx(SockAddr const& fromHost, Seq fromMailbox) : m_fromHost(fromHost), m_fromMailbox(fromMailbox) {}

		SmtpReceiveInstruction SmtpReceiverAuthCx_OnMailFrom_HaveAuth (Seq fromMailbox                       ) override final;
		SmtpReceiveInstruction SmtpReceiverAuthCx_OnRcptTo            (Seq toMailbox                         ) override final;
		SmtpReceiveInstruction SmtpReceiverAuthCx_OnData              (Vec<Str> const& toMailboxes, Seq data ) override final;
	};
};


void AutSmtpReceiver::WorkPool_LogEvent(WORD eventType, Seq text)
{
	Console::Out(Str::From(Time::StrictNow(), TimeFmt::IsoMicroZ).Add(" ").Add(LogEventType::Desc(eventType)).Add(": ").Add(text).SetEndExact("\r\n"));
}


void AutSmtpReceiver::SmtpReceiver_GetCfg(SmtpReceiverCfg& cfg) const
{
	static Str s_token = Token::Generate();

	EmailSrvBinding& b = cfg.f_bindings.Clear().Add();
	b.f_token = s_token;
	b.f_intf = "127.0.0.1";
	b.f_port = 25;
	b.f_ipv6Only = false;
	b.f_implicitTls = false;
	b.f_computerName.Clear();
	b.f_maxInMsgKb = 0;
	b.f_desc.Clear();

	cfg.f_computerName = "127.0.0.1";
	cfg.f_maxInMsgKb = 10*1000;
}


SmtpReceiveInstruction AutSmtpReceiver::RandomReply(std::function<void(char const*)> action)
{
	uint        replyCode {};
	char const* replyText { "OK" };

	switch (Crypt::GenRandomNr(9))
	{
	case 0: replyCode = 500; replyText = "Permanent failure"; break;
	case 1: replyCode = 400; replyText = "Temporary failure"; break;
	default: break;
	}

	action(replyText);

	if (!replyCode)
		return SmtpReceiveInstruction::Accept(MaxInMsgBytes);

	return SmtpReceiveInstruction::Refuse(replyCode, replyText);
}


SmtpReceiveInstruction AutSmtpReceiver::SmtpReceiver_OnMailFrom_NoAuth(SockAddr const& fromHost, Schannel&, Seq, EhloHost const&,
			Seq fromMailbox, Rp<SmtpReceiverAuthCx>& authCx)
{
	SmtpReceiveInstruction instr = RandomReply([&] (char const* replyText)
		{ Console::Out(Str::From(Time::StrictNow(), TimeFmt::IsoMicroZ).Add(" ").Obj(fromHost, SockAddr::AddrPort).Add(": Mail from ").Add(fromMailbox)
			.Add(", reply: ").Add(replyText).Add("\r\n")); } );

	if (instr.m_accept)
		authCx = new AuthCx(fromHost, fromMailbox);

	return instr;
}


SmtpReceiveInstruction AutSmtpReceiver::AuthCx::SmtpReceiverAuthCx_OnMailFrom_HaveAuth(Seq fromMailbox)
{
	m_fromMailbox = fromMailbox;
	return RandomReply([&] (char const* replyText)
		{ Console::Out(Str::From(Time::StrictNow(), TimeFmt::IsoMicroZ).Add(" ").Obj(m_fromHost, SockAddr::AddrPort).Add(": Mail from ").Add(fromMailbox)
			.Add(", reply: ").Add(replyText).Add("\r\n")); } );
}


SmtpReceiveInstruction AutSmtpReceiver::AuthCx::SmtpReceiverAuthCx_OnRcptTo(Seq toMailbox)
{
	return RandomReply([&] (char const* replyText)
		{ Console::Out(Str::From(Time::StrictNow(), TimeFmt::IsoMicroZ).Add(" ").Obj(m_fromHost, SockAddr::AddrPort).Add(": Mail from ").Add(m_fromMailbox)
			.Add(", rcpt to: ").Add(toMailbox).Add(", reply: ").Add(replyText).Add("\r\n")); } );
}


SmtpReceiveInstruction AutSmtpReceiver::AuthCx::SmtpReceiverAuthCx_OnData(Vec<Str> const& toMailboxes, Seq data)
{
	Str toStr;
	for (Str const& mbox : toMailboxes)
		toStr.IfAny("; ").Add(mbox);

	return RandomReply([&] (char const* replyText)
		{ Console::Out(Str::From(Time::StrictNow(), TimeFmt::IsoMicroZ).Add(" ").Obj(m_fromHost, SockAddr::AddrPort).Add(": Mail from ").Add(m_fromMailbox)
			.Add(", to: ").Add(toStr).Add(", data size: ").UInt(data.n).Add(", reply: ").Add(replyText).Add("\r\n")); } );
}



// SmtpReceiverTest

#pragma warning (disable: 4702) // Spurious unreachable code warning

void SmtpReceiverTest()
{
	try
	{
		Crypt::Initializer cryptInit;

		Rp<StopCtl>        stopCtl { new StopCtl };
		ThreadPtr<WaitEsc> waitEsc { Thread::Create };
		waitEsc->Start(stopCtl);

		ThreadPtr<AutSmtpReceiver> receiver { Thread::Create };
		receiver->SetStopCtl(stopCtl);
		receiver->Start();

		stopCtl->WaitAll();
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Terminated by std::exception:\r\n")
					.Add(e.what()).Add("\r\n"));
	}
}
