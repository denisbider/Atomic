#pragma once

#include "AtAbortable.h"
#include "AtSocket.h"

#include "AtDnsQuery.h"


namespace At
{
	class SocketConnector : virtual public Abortable
	{
	public:
		struct Err : CommunicationErr { Err(Seq msg) : CommunicationErr(msg) {} };

		struct LookupErr : Err
		{
			LookupErr(Seq host, Seq msg)
				: Err(Str("Could not look up ").Add(host).Add(": ").Add(msg))
				, m_host(host) {}

			Str m_host;
		};

		struct LookupWinErr : LookupErr
		{
			LookupWinErr(Seq host, Seq msg, int rc)
				: LookupErr(host, Str(msg).Add(": ").Fun(DescribeWinErr, rc))
				, m_errCode(rc) {}

			int m_errCode;
		};

		struct ConnectWinErr : Err
		{
			ConnectWinErr(SockAddr sa, Seq msg, int rc)
				: Err(Str("Error connecting to ").Obj(sa, SockAddr::AddrPort).Add(": ").Add(msg).Add(": ").Fun(DescribeWinErr, rc))
				, m_sa(sa), m_errCode(rc) {}
			
			SockAddr m_sa;
			int m_errCode;
		};

		SOCKET ConnectWithLookup(Seq hostPort, uint defaultPort, IpVerPreference::E ipVerPreference, std::function<void (Socket&)> configureSocket = nullptr);
		SOCKET ConnectToAddr(SockAddr const& sa, std::function<void (Socket&)> configureSocket = nullptr);
	};
}
