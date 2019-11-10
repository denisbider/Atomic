#include "AtIncludes.h"
#include "AtPop3ServerThread.h"
#include "AtSmtpReceiverThread.h"

#include "AtNumCvt.h"
#include "AtWinErr.h"


namespace At
{

	// EmailServer

	template <class ThreadType>
	void EmailServer<ThreadType>::WorkPool_Run()
	{
		SockInit sockInit;

		struct Binding
		{
			Str    m_bindingToken;
			Socket m_listener;
		};

		enum { NrBaseEvents = 2 };
		Vec<Binding> bindings;
		Vec<HANDLE> waitHandles;
		bool reloadBindings = true;

		while (true)
		{
			if (reloadBindings)
			{
				reloadBindings = false;

				EntVec<EmailSrvBinding> const& cfgBindings = EmailServer_GetCfgBindings();

				bindings.Clear();
				bindings.ReserveExact(cfgBindings.Len());

				waitHandles.Clear();
				waitHandles.ReserveExact(NrBaseEvents + bindings.Len());
				waitHandles.Add(StopEvent().Handle());
				waitHandles.Add(m_reloadBindingsTrigger.Handle());
				EnsureThrow(NrBaseEvents == waitHandles.Len());

				for (EmailSrvBinding const& esb : cfgBindings)
				{
					SockAddr sa;
					sa.Parse(esb.f_intf);
					if (!sa.IsIp4Or6())
						WorkPool_LogEvent(LogEventType::Warning, Str::Join("Binding ignored - invalid interface: ", esb.f_intf));
					else if (esb.f_port < 1 || esb.f_port > 65535)
						WorkPool_LogEvent(LogEventType::Warning, Str("Binding ignored - invalid port: ").UInt(esb.f_port));
					else
					{
						sa.SetPort((uint16) esb.f_port);

						Binding& b = bindings.Add();
						b.m_bindingToken = esb.f_token;

						if (sa.IsIp4())
							b.m_listener.Create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						else
						{
							b.m_listener.Create(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							b.m_listener.SetIpv6Only(esb.f_ipv6Only);
						}

						b.m_listener.Listen(sa);
						waitHandles.Add(b.m_listener.AcceptEvent().Handle());

						WorkPool_LogEvent(LogEventType::Info, Str("Waiting for connections on ").Obj(sa, SockAddr::AddrPort));
					}
				}
			}

			DWORD rc = WaitHandles(NumCast<DWORD>(waitHandles.Len()), waitHandles.Ptr(), INFINITE);
			if (rc == 0) return;
			if (rc == 1) { reloadBindings = true; continue; }
			if (rc >= waitHandles.Len()) throw ErrWithCode<>(rc, "Unexpected return value from WaitForMultipleObjects");

			Binding& b = bindings[rc - NrBaseEvents];
			AutoFree<EmailServerWorkItem> workItem = new EmailServerWorkItem;
			b.m_listener.Accept(workItem->m_sk, workItem->m_saRemote);
			workItem->m_bindingToken = b.m_bindingToken;

			EnqueueWorkItem(workItem);
		}
	}


	template class EmailServer<Pop3ServerThread>;
	template class EmailServer<SmtpReceiverThread>;



	// EmailServer_ClientLine

	void EmailServer_ClientLine::ReadLine(Reader& reader)
	{
		reader.SetExpireMs(EmailServer_RecvTimeoutMs);
		reader.Read( [&] (Seq& avail) -> Reader::Instr::E
			{
				Seq const line { avail.ReadToString("\r\n") };
				if (avail.n)
				{
					m_line = line;

					avail.DropBytes(2);
					return Reader::Instr::Done;
				}
		
				if (avail.n > EmailServer_MaxClientLineBytes)
					throw EmailServer_Disconnect("Line too long");

				return Reader::Instr::NeedMore;		
			} );
	}



	// EmailServer_ClientCmd

	void EmailServer_ClientCmd::ReadCmd(Reader& reader)
	{
		ReadLine(reader);
		Seq lineReader = m_line;
		m_cmd = lineReader.ReadToByte(' ') >> ToLower;
		if (lineReader.n)
			m_params = lineReader.Trim();
	}

}
