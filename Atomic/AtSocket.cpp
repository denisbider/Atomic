#include "AtIncludes.h"
#include "AtSocket.h"

#include "AtDllNtDll.h"
#include "AtException.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	// SockInit

	SockInit::SockInit()
	{
		WSADATA data {};
		int rc { WSAStartup(0x0202, &data) };
		if (rc != 0)
			{ LastWsaErr e; throw e.Make<>("AtSockInit: Error in WSAStartup"); }
	}

	SockInit::~SockInit()
	{
		EnsureReportWithNr(WSACleanup() == 0, WSAGetLastError());
	}



	// SockAddr

	Seq const SockAddr::sc_ip4MappedIp6Prefix { "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff", 12 };

	SockAddr& SockAddr::ParseIp4(wchar_t const* wz)
	{
		ZeroMemory(&m_sa, sizeof(m_sa));
	
		PCWSTR terminator;
		if (Call_RtlIpv4StringToAddressW(wz, true, &terminator, &m_sa.sa4.sin_addr) == NO_ERROR && !*terminator)
		{
			m_saLen = sizeof(sockaddr_in);
			m_sa.family = AF_INET;
			return (m_valid = true), *this;
		}
	
		return (m_valid = false), *this;
	}


	SockAddr& SockAddr::ParseIp6(wchar_t const* wz)
	{
		ZeroMemory(&m_sa, sizeof(m_sa));
	
		PCWSTR terminator;
		if (Call_RtlIpv6StringToAddressW(wz, &terminator, &m_sa.sa6.sin6_addr) == NO_ERROR && !*terminator)
		{
			m_saLen = sizeof(sockaddr_in6);
			m_sa.family = AF_INET6;
			return (m_valid = true), *this;
		}
	
		return (m_valid = false), *this;
	}


	SockAddr& SockAddr::Parse(Seq s)
	{
		WinStr wz { s };
		ParseIp4(wz.Z());
		if (!m_valid)
			ParseIp6(wz.Z());
		return *this;
	}


	SockAddr& SockAddr::ParseWithPort(Seq s, uint defaultPort)
 	{
		EnsureThrow(defaultPort <= 65535);	
		Str addrPart = Str::NullTerminate(s.ReadToByte(':'));
		LPCSTR terminator = nullptr;
		LONG retVal = Call_RtlIpv6StringToAddressA(addrPart.CharPtr(), &terminator, &m_sa.sa6.sin6_addr);
		if (retVal == NO_ERROR)
 		{
			m_saLen = sizeof(sockaddr_in6);
			m_sa.family = AF_INET6;
 		}
 		else
 		{
			retVal = Call_RtlIpv4StringToAddressA(addrPart.CharPtr(), TRUE, &terminator, &m_sa.sa4.sin_addr);
			if (retVal == NO_ERROR)
			{
				m_saLen = sizeof(sockaddr_in);
				m_sa.family = AF_INET;
			}
			else
				return (m_valid = false), *this;
		}
 
		if (s.ReadByte() != ':' || !s.n)
			m_sa.sa4.sin_port = htons((u_short) defaultPort);
		else
			m_sa.sa4.sin_port = htons((u_short) s.ReadNrUInt16Dec());
 
		return (m_valid = true), *this;
 	}


	SockAddr& SockAddr::Set(sockaddr const* sa, sizet saLen)
	{
		if (!sa || (sa->sa_family != AF_INET && sa->sa_family != AF_INET6))
			return (m_valid = false), *this;

		if (saLen == Auto)
			saLen = (sa->sa_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
		else
		{
			if (saLen > sizeof(m_sa))										return (m_valid = false), *this;
			if (sa->sa_family == AF_INET && saLen < sizeof(sockaddr_in))	return (m_valid = false), *this;
			if (sa->sa_family == AF_INET6 && saLen < sizeof(sockaddr_in6))	return (m_valid = false), *this;
		}
	
		m_saLen = NumCast<int>(saLen);
		memcpy(&m_sa, sa, saLen);
		return (m_valid = true), *this;
	}


	SockAddr& SockAddr::SetIp4(uint32 ip4Nr, uint16 port)
	{
		ZeroMemory(&m_sa, sizeof(m_sa));
		m_saLen = sizeof(sockaddr_in);
		m_sa.sa4.sin_family = AF_INET;
		m_sa.sa4.sin_addr.S_un.S_un_b.s_b1 = (byte) ((ip4Nr >> 24) & 0xFF);
		m_sa.sa4.sin_addr.S_un.S_un_b.s_b2 = (byte) ((ip4Nr >> 16) & 0xFF);
		m_sa.sa4.sin_addr.S_un.S_un_b.s_b3 = (byte) ((ip4Nr >>  8) & 0xFF);
		m_sa.sa4.sin_addr.S_un.S_un_b.s_b4 = (byte) ((ip4Nr      ) & 0xFF);
		m_sa.sa4.sin_port = htons(port);
		return (m_valid = true), *this;
	}


	SockAddr& SockAddr::SetIp4(IN_ADDR const& addr, uint16 port)
	{
		ZeroMemory(&m_sa, sizeof(m_sa));
		m_saLen = sizeof(sockaddr_in);
		m_sa.sa4.sin_family = AF_INET;
		m_sa.sa4.sin_addr = addr;
		m_sa.sa4.sin_port = htons(port);
		return (m_valid = true), *this;
	}


	SockAddr& SockAddr::SetIp6(Seq ip6Bin, uint16 port, ULONG scopeId)
	{
		EnsureThrow(ip6Bin.n == 16);
		ZeroMemory(&m_sa, sizeof(m_sa));
		m_saLen = sizeof(sockaddr_in6);
		m_sa.sa6.sin6_family = AF_INET6;
		memcpy(&m_sa.sa6.sin6_addr.u.Byte[0], ip6Bin.p, 16);
		m_sa.sa6.sin6_port = htons(port);
		m_sa.sa6.sin6_scope_id = scopeId;
		return (m_valid = true), *this;
	}


	SockAddr& SockAddr::SetIp6(IN6_ADDR const& addr, uint16 port, ULONG scopeId)
	{
		ZeroMemory(&m_sa, sizeof(m_sa));
		m_saLen = sizeof(sockaddr_in6);
		m_sa.sa6.sin6_family = AF_INET6;
		m_sa.sa6.sin6_addr = addr;
		m_sa.sa6.sin6_port = htons(port);
		m_sa.sa6.sin6_scope_id = scopeId;
		return (m_valid = true), *this;
	}


	SockAddr& SockAddr::SetIp6FromIp4(SockAddr const& x)
	{
		if (x.IsIp4MappedIp6())
			{ *this = x; return *this; }

		EnsureThrow(x.IsIp4());
		ZeroMemory(&m_sa, sizeof(m_sa));
		m_saLen = sizeof(sockaddr_in6);
		m_sa.sa6.sin6_family = AF_INET6;
		memcpy(m_sa.sa6.sin6_addr.u.Byte,    sc_ip4MappedIp6Prefix.p,          sc_ip4MappedIp6Prefix.n );
		memcpy(m_sa.sa6.sin6_addr.u.Byte+12, &x.m_sa.sa4.sin_addr.S_un.S_addr, 4                       );
		m_sa.sa6.sin6_port = m_sa.sa4.sin_port;
		m_sa.sa6.sin6_scope_id = 0;
		return (m_valid = true), *this;
	}

		
	SockAddr& SockAddr::SetIp6Any(uint16 port, ULONG scopeId)
	{
		return SetIp6(Seq("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16), port, scopeId);
	}


	SockAddr& SockAddr::SetIp6Local(uint16 port, ULONG scopeId)
	{
		return SetIp6(Seq("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01", 16), port, scopeId);
	}


	SockAddr& SockAddr::SetToIdAddressOf(SockAddr const& x)
	{
		EnsureThrow(&x != this);
		EnsureThrow(x.IsIp4Or6());

		if (x.m_sa.family == AF_INET)
		{
			// IPv4 address
			ZeroMemory(&m_sa, sizeof(m_sa));
			m_saLen = sizeof(sockaddr_in);
			m_sa.sa4.sin_family = AF_INET;
			m_sa.sa4.sin_addr.S_un.S_addr = x.m_sa.sa4.sin_addr.S_un.S_addr;
			return (m_valid = true), *this;
		}
		else if (Seq(&x.m_sa.sa6.sin6_addr, 16).StartsWithExact(sc_ip4MappedIp6Prefix))
		{
			// IPv4-Mapped IPv6 address
			ZeroMemory(&m_sa, sizeof(m_sa));
			m_saLen = sizeof(sockaddr_in6);
			m_sa.sa6.sin6_family = AF_INET6;
			m_sa.sa6.sin6_addr = x.m_sa.sa6.sin6_addr;
			return (m_valid = true), *this;
		}
		else
		{
			// IPv6 address
			ZeroMemory(&m_sa, sizeof(m_sa));
			m_saLen = sizeof(sockaddr_in6);
			m_sa.sa6.sin6_family = AF_INET6;
			m_sa.sa6.sin6_addr = x.m_sa.sa6.sin6_addr;
			ZeroMemory(&m_sa.sa6.sin6_addr.u.Byte[8], 8);
			return (m_valid = true), *this;
		}
	}


	SockAddr& SockAddr::SetToEndOfSubNet(SockAddr const& x, uint sigBits)
	{
		EnsureThrow(&x != this);
		EnsureThrow(x.IsIp4Or6());

		if (x.m_sa.family == AF_INET)
		{
			uint32 ip = x.GetIp4Nr();

			if (sigBits == 0)
				ip = UINT32_MAX;
			else if (sigBits < 32)
			{
				uint32 mask = (1U << (32U - sigBits)) - 1U;
				ip |= mask;
			}
			
			ZeroMemory(&m_sa, sizeof(m_sa));
			m_saLen = sizeof(sockaddr_in);
			m_sa.sa4.sin_family = AF_INET;
			m_sa.sa4.sin_addr.S_un.S_addr = htonl(ip);
			m_sa.sa4.sin_port = x.m_sa.sa4.sin_port;
			return (m_valid = true), *this;
		}
		else
		{
			ZeroMemory(&m_sa, sizeof(m_sa));
			m_saLen = sizeof(sockaddr_in6);
			m_sa.sa6.sin6_family = AF_INET6;
			m_sa.sa6.sin6_addr = x.m_sa.sa6.sin6_addr;
			m_sa.sa6.sin6_port = x.m_sa.sa6.sin6_port;
			m_sa.sa6.sin6_scope_id = x.m_sa.sa6.sin6_scope_id;

			if (sigBits < 128)
			{
				uint partBytePos = sigBits / 8;
				uint sigBitsInPartByte = sigBits % 8;

				uint mask = (1U << (8U - sigBitsInPartByte)) - 1U;
				m_sa.sa6.sin6_addr.u.Byte[partBytePos] |= mask;

				for (uint i=partBytePos+1; i!=16; ++i)
					m_sa.sa6.sin6_addr.u.Byte[i] = 0xFF;
			}

			return (m_valid = true), *this;
		}
	}


	SockAddr& SockAddr::IncrementIp()
	{
		EnsureThrow(IsIp4Or6());
		if (m_sa.family == AF_INET)
			m_sa.sa4.sin_addr.S_un.S_addr = htonl(ntohl(m_sa.sa4.sin_addr.S_un.S_addr) + 1);
		else
		{
			for (int i=15; i>=0; --i)
				if (++m_sa.sa6.sin6_addr.u.Byte[i] != 0)
					break;
		}
		return *this;
	}


	SockAddr& SockAddr::SetPort(uint16 port)
	{
		EnsureThrow(IsIp4Or6());
		if (m_sa.family == AF_INET)
			m_sa.sa4.sin_port = htons((u_short) port);
		else
			m_sa.sa6.sin6_port = htons((u_short) port);
		return *this;
	}


	uint SockAddr::IpVer() const
	{
		if (!m_valid) return 0;
		if (m_sa.family == AF_INET) return 4;
		if (m_sa.family == AF_INET6) return 6;
		return 0;
	}


	void SockAddr::EncObj(Enc& enc, EAddrOnly) const
	{
		EnsureThrow(IsIp4Or6());
		Enc::Write write = enc.IncWrite(MaxEncLen);
		char* endPtr;

		if (m_sa.family == AF_INET)
			endPtr = Call_RtlIpv4AddressToStringA(&m_sa.sa4.sin_addr, write.CharPtr());
		else
			endPtr = Call_RtlIpv6AddressToStringA(&m_sa.sa6.sin6_addr, write.CharPtr());

		write.AddSigned(endPtr - write.CharPtr());
	}


	void SockAddr::EncObj(Enc& enc, EAddrPort) const
	{
		EnsureThrow(IsIp4Or6());
		Enc::Write write = enc.IncWrite(MaxEncLen);
		ULONG outLen = MaxEncLen - 1;
		LONG rc;

		if (m_sa.family == AF_INET)
			rc = Call_RtlIpv4AddressToStringExA(&m_sa.sa4.sin_addr, m_sa.sa4.sin_port, write.CharPtr(), &outLen);
		else
			rc = Call_RtlIpv6AddressToStringExA(&m_sa.sa6.sin6_addr, m_sa.sa6.sin6_scope_id, m_sa.sa6.sin6_port, write.CharPtr(), &outLen);
		if (rc != NO_ERROR)
			{ LastWinErr e; throw e.Make<>("Error in RtlIpv#AddressToStringExA"); }

		if (outLen && !write.Ptr()[outLen-1])
			--outLen;

		write.Add(outLen);
	}


	uint SockAddr::GetIpLen() const
	{
		EnsureThrow(m_valid);
		if (m_sa.family == AF_INET)
			return 4;

		EnsureThrow(m_sa.family == AF_INET6);
		return 16;
	}


	uint32 SockAddr::GetIp4Nr() const
	{
		EnsureThrow(m_valid);
		if (m_sa.family == AF_INET)
			return ntohl(m_sa.sa4.sin_addr.S_un.S_addr);

		EnsureThrow(IsIp4MappedIp6());
		return ntohl(*((uint32 const*) &m_sa.sa6.sin6_addr.u.Byte[12]));
	}


	uint16 SockAddr::GetPort() const
	{
		EnsureThrow(m_valid);
		if (m_sa.family == AF_INET)
			return ntohs(m_sa.sa4.sin_port);

		EnsureThrow(m_sa.family == AF_INET6);
		return ntohs(m_sa.sa6.sin6_port);
	}


	Seq SockAddr::GetIpBin() const
	{
		EnsureThrow(m_valid);
		if (m_sa.family == AF_INET)
			return Seq(&m_sa.sa4.sin_addr.S_un.S_addr, 4);

		EnsureThrow(m_sa.family == AF_INET6);
		return Seq(&m_sa.sa6.sin6_addr.u.Byte[0], 16);
	}


	uint64 SockAddr::GetIp6Hi() const
	{
		EnsureThrow(IsIp6());
		return	(((uint64) m_sa.sa6.sin6_addr.u.Byte[0]) << 56) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[1]) << 48) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[2]) << 40) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[3]) << 32) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[4]) << 24) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[5]) << 16) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[6]) <<  8) |
				(((uint64) m_sa.sa6.sin6_addr.u.Byte[7])      );
	}


	bool SockAddr::IsLocal() const
	{
		return m_valid &&
			   (m_sa.family == AF_INET  && m_sa.sa4.sin_addr.S_un.S_un_b.s_b1 == 127) ||
			   (m_sa.family == AF_INET6 && !memcmp(&m_sa.sa6.sin6_addr, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01", 16));
	}


	bool SockAddr::IsIpMaxValue() const
	{
		return m_valid &&
			   (m_sa.family == AF_INET  && m_sa.sa4.sin_addr.S_un.S_addr == 0xFFFFFFFF) ||
			   (m_sa.family == AF_INET6 && !memcmp(&m_sa.sa6.sin6_addr, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16));
	}


	SockAddr const* SockAddr::SetIp6FromIp4_IfNotAlreadyIp6(SockAddr const& orig, SockAddr& storage)
	{
		EnsureThrow(orig.IsIp4Or6());
		if (orig.m_sa.family == AF_INET6)
			return &orig;

		storage.SetIp6FromIp4(orig);
		return &storage;
	}


	int SockAddr::Compare(SockAddr const& x, uint flags) const
	{
		// Any invalid address is less than a valid address. Two invalid addresses are equal
		if (!m_valid) return x.m_valid ? -1 : 0;
		if (!x.m_valid) return 1;

		// Are both addresses IPv4?
		if (m_sa.family == AF_INET && x.m_sa.family == AF_INET)
		{
			uint32 left  =   GetIp4Nr();
			uint32 right = x.GetIp4Nr();
			if (left < right) return -4;
			if (left > right) return  4;

			if (0 == (flags & SockAddrCmp::AddrOnly))
			{
				uint16 leftPort  = ntohs(  m_sa.sa4.sin_port);
				uint16 rightPort = ntohs(x.m_sa.sa4.sin_port);
				if (leftPort < rightPort) return -5;
				if (leftPort > rightPort) return  5;
			}

			return 0;
		}
		else
		{
			// One or both are IPv6. Convert any that's IPv4 to mapped IPv6
			SockAddr leftStorage, rightStorage;
			SockAddr const* left  = SetIp6FromIp4_IfNotAlreadyIp6(*this, leftStorage  );
			SockAddr const* right = SetIp6FromIp4_IfNotAlreadyIp6(x,     rightStorage );

			int diff = memcmp(&left->m_sa.sa6.sin6_addr, &right->m_sa.sa6.sin6_addr, 16);
			if (diff < 0) return -6;
			if (diff > 0) return  6;

			if (0 == (flags & SockAddrCmp::AddrOnly))
			{
				uint16 leftPort  = ntohs(left ->m_sa.sa6.sin6_port);
				uint16 rightPort = ntohs(right->m_sa.sa6.sin6_port);
				if (leftPort < rightPort) return -7;
				if (leftPort > rightPort) return  7;

				ULONG leftScopeId  = left ->m_sa.sa6.sin6_scope_id;
				ULONG rightScopeId = right->m_sa.sa6.sin6_scope_id;
				if (leftScopeId < rightScopeId) return -8;
				if (leftScopeId > rightScopeId) return  8;
			}

			return 0;
		}
	}


	int SockAddr::CompareStr_AddrOnly(Seq s) const
	{
		SockAddr right;
		right.Parse(s);
		return Compare(right, SockAddrCmp::AddrOnly);
	}



	// Socket

	Socket::Socket(Socket&& x) noexcept
		: m_s                     (x.m_s           )
		, m_localSa     (std::move(x.m_localSa     ))
		, m_remoteSa    (std::move(x.m_remoteSa    ))
		, m_listening             (x.m_listening   )
		, m_acceptEvent (std::move(x.m_acceptEvent ))
	{
		x.m_s = INVALID_SOCKET;
		x.m_localSa = SockAddr();
		x.m_remoteSa = SockAddr();
		x.m_listening = false;
	}


	Socket::~Socket() noexcept
	{
		if (m_s != INVALID_SOCKET)
			EnsureReportWithNr(closesocket(m_s) == 0, WSAGetLastError());
	}


	void Socket::Create(int af, int type, int protocol)
	{
		EnsureThrow(m_s == INVALID_SOCKET);
		m_s = WSASocket(af, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
		if (m_s == INVALID_SOCKET)
			{ LastWsaErr e; throw e.Make<Err>("Error in WSASocket"); }
	}


	void Socket::SetSocket(SOCKET s)
	{
		EnsureThrow(m_s == INVALID_SOCKET);
		m_s = s;
	}


	void Socket::Close()
	{
		if (m_s != INVALID_SOCKET)
		{
			if (closesocket(m_s) != 0)
				{ LastWsaErr e; throw e.Make<Err>("Error in closesocket"); }

			m_s = INVALID_SOCKET;
		}
	}


	void Socket::Bind(SockAddr const& sa)
	{
		EnsureThrow(!m_listening);
		EnsureThrow(sa.m_valid);
		EnsureThrow(m_s != INVALID_SOCKET);

		if (::bind(m_s, &sa.m_sa.sa, sa.m_saLen) != 0)
			{ LastWsaErr e; throw e.Make<Err>("Error in bind"); }
	}

	void Socket::SetSockOpt(int level, int optname, int val)
	{
		EnsureThrow(m_s != INVALID_SOCKET);
		int rc { setsockopt(m_s, level, optname, (char const*) &val, sizeof(int)) };
		if (rc != 0)
			{ LastWsaErr e; throw e.Make<Err>("Error in setsockopt"); }
	}


	void Socket::Listen(SockAddr const& sa, int backlog)
	{
		EnsureThrow(!m_listening);
		EnsureThrow(sa.m_valid);
		EnsureThrow(m_s != INVALID_SOCKET);

		m_listening = true;
		m_acceptEvent.Create(Event::CreateManual);

		if (::bind(m_s, &sa.m_sa.sa, sa.m_saLen) != 0)
			{ LastWsaErr e; throw e.Make<Err>("Error in bind"); }

		if (listen(m_s, backlog) != 0)
			{ LastWsaErr e; throw e.Make<Err>("Error in listen"); }

		if (WSAEventSelect(m_s, m_acceptEvent.Handle(), FD_ACCEPT) != 0)
			{ LastWsaErr e; throw e.Make<Err>("Error in WSAEventSelect for listening socket"); }
	}


	void Socket::Accept(Socket& newSk, SockAddr& remoteSa)
	{
		EnsureThrow(m_listening);
		EnsureThrow(newSk.m_s == INVALID_SOCKET);

		m_acceptEvent.ClearSignal();

		remoteSa.m_saLen = sizeof(remoteSa.m_sa);
		newSk.m_s = accept(m_s, &remoteSa.m_sa.sa, &remoteSa.m_saLen);
		if (newSk.m_s == INVALID_SOCKET)
			{ LastWsaErr e; throw e.Make<Err>("Error in accept"); }

		remoteSa.m_valid = true;
	}


	void Socket::InitLocalAddr() const
	{
		EnsureThrow(!m_localSa.m_valid);
	
		SOCKADDR_STORAGE saStorage;
		int saLen = sizeof(saStorage);
		sockaddr* sa = (sockaddr*) &saStorage;

		if (getsockname(m_s, sa, &saLen) == SOCKET_ERROR)
			{ LastWsaErr e; throw e.Make<Err>("Error in getsockname"); }

		m_localSa.Set(sa, (sizet) saLen);
	}


	void Socket::InitRemoteAddr() const
	{
		EnsureThrow(!m_remoteSa.m_valid);
	
		SOCKADDR_STORAGE saStorage;
		int saLen = sizeof(saStorage);
		sockaddr* sa = (sockaddr*) &saStorage;

		if (getpeername(m_s, sa, &saLen) == SOCKET_ERROR)
			{ LastWsaErr e; throw e.Make<Err>("Error in getpeername"); }

		m_remoteSa.Set(sa, (sizet) saLen);
	}

}
