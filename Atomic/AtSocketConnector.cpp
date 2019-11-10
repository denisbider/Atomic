#include "AtIncludes.h"
#include "AtSocketConnector.h"


namespace At
{

	SOCKET SocketConnector::ConnectWithLookup(Seq hostPort, uint defaultPort, IpVerPreference::E ipVerPreference, std::function<void (Socket&)> configureSocket)
	{
		Seq  host { hostPort.ReadToByte(':') };
		uint port { defaultPort };
		if (hostPort.n)
			port = hostPort.DropByte().ReadNrUInt32Dec();

		if (port == 0 || port > 65535)
			throw StrErr(Str(__FUNCTION__ ": Invalid port number: ").UInt(port));

		ThreadPtr<AsyncAddrLookup> aal { Thread::Create };
		aal->m_nodeName.Set(host);
		aal->m_serviceName.UInt(port);
		aal->m_useHints = true;
		aal->m_hints.ai_family   = AF_UNSPEC;
		aal->m_hints.ai_socktype = SOCK_STREAM;
		aal->m_hints.ai_protocol = IPPROTO_TCP;
		aal->SetAbortableParams(*this);
		aal->Start().Join();
		if (aal->m_rc != 0)
			throw LookupWinErr(host, "Error in GetAddrInfo", aal->m_rc);

		if (!aal->m_result)
			throw LookupErr(host, "GetAddrInfo returned no results");

		Map<LookedUpAddr> addrs;
		ProcessAddrInfoResults(aal->m_result, host, ipVerPreference, 0, addrs);

		Map<LookedUpAddr>::ConstIt addrIt = addrs.begin();
		EnsureThrow(addrIt.Any());
		while (true)
		{
			try { return ConnectToAddr(addrIt->m_sa, configureSocket); }
			catch (ConnectWinErr const&)
			{
				++addrIt;
				if (!addrIt.Any())
					throw;
			}
		}
	}


	SOCKET SocketConnector::ConnectToAddr(SockAddr const& sa, std::function<void (Socket&)> configureSocket)
	{
		EnsureThrow(sa.IsIp4Or6());

		Socket sk;
		int af { If(sa.IsIp4(), int, AF_INET, AF_INET6) };
		sk.Create(af, SOCK_STREAM, IPPROTO_TCP);

		if (!!configureSocket)
			configureSocket(sk);

		Event connEvent(Event::CreateAuto);
		if (WSAEventSelect(sk.GetSocket(), connEvent.Handle(), FD_CONNECT) != 0)
			throw ConnectWinErr(sa, "Error in WSAEventSelect", WSAGetLastError());

		int connectResult { connect(sk.GetSocket(), &sa.m_sa.sa, sa.m_saLen) };
		if (connectResult != 0)
		{
			connectResult = WSAGetLastError();
			if (connectResult != WSAEWOULDBLOCK)
				throw ConnectWinErr(sa, "Error in connect (immediate)", connectResult);

			WSANETWORKEVENTS events;
			do
			{
				AbortableWait(connEvent.Handle());
				WSAEnumNetworkEvents(sk.GetSocket(), connEvent.Handle(), &events);
			}
			while ((events.lNetworkEvents & FD_CONNECT) == 0);

			connectResult = events.iErrorCode[FD_CONNECT_BIT];
			if (connectResult != 0)
				throw ConnectWinErr(sa, "Error in connect (delayed)", connectResult);
		}

		return sk.ReleaseSocket();
	}

}
