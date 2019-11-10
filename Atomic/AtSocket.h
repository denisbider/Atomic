#pragma once

#include "AtIncludes.h"
#include "AtEvent.h"
#include "AtNumCvt.h"
#include "AtStr.h"

namespace At
{
	class SockInit : NoCopy
	{
	public:
		SockInit();
		~SockInit();
	};


	struct SockAddr
	{
		static Seq const sc_ip4MappedIp6Prefix;

		bool m_valid {};
		int  m_saLen {};

		union
		{
			short family;
			sockaddr sa;
			sockaddr_in sa4;
			sockaddr_in6 sa6;
			SOCKADDR_STORAGE storage;
		} m_sa;

		enum { Auto = 0 };

		SockAddr& ParseIp4(wchar_t const* wz);
		SockAddr& ParseIp6(wchar_t const* wz);
		SockAddr& Parse(Seq s);
		SockAddr& ParseWithPort(Seq s, uint defaultPort);

		SockAddr& Set(sockaddr const* sa, sizet saLen);		// Specify saLen=Auto to set it automatically based on sa->sa_family
		SockAddr& Set(addrinfo const& ai)  { return Set(ai.ai_addr, ai.ai_addrlen); }
		SockAddr& Set(addrinfoW const& ai) { return Set(ai.ai_addr, ai.ai_addrlen); }

		SockAddr& SetIp4(uint32 ip4Nr, uint16 port);
		SockAddr& SetIp4(IN_ADDR const& addr, uint16 port);

		SockAddr& SetIp6(Seq ip6Bin, uint16 port, ULONG scopeId);
		SockAddr& SetIp6(IN6_ADDR const& addr, uint16 port, ULONG scopeId);
		SockAddr& SetIp6FromIp4(SockAddr const& ip4);
		SockAddr& SetIp6Any(uint16 port, ULONG scopeId);
		SockAddr& SetIp6Local(uint16 port, ULONG scopeId);

		SockAddr& SetToIdAddressOf(SockAddr const& x);	// For IPv4 and IPv4-Mapped IPv6, copies address and sets port to 0. For IPv6, nulls least significant 64 bits of address.
		SockAddr& SetToEndOfSubNet(SockAddr const& x, uint sigBits);

		SockAddr& IncrementIp();
	
		SockAddr& SetPort(uint16 port);
	
		bool Valid() const { return m_valid; }

		uint IpVer() const;
		bool IsIp4() const { return m_valid && m_sa.family == AF_INET; }
		bool IsIp6() const { return m_valid && m_sa.family == AF_INET6; }
		bool IsIp4Or6() const { return m_valid && m_sa.family == AF_INET || m_sa.family == AF_INET6; }
		bool IsIp4MappedIp6() const { return m_valid && m_sa.family == AF_INET6 && IsIp4MappedIp6(Seq(&m_sa.sa6.sin6_addr, 16)); }

		static bool IsIp4MappedIp6(Seq s) { return s.n == 16 && s.StartsWithExact(sc_ip4MappedIp6Prefix); }

		enum { MaxEncLen = 60 };
		enum EAddrOnly { AddrOnly };
		enum EAddrPort { AddrPort };

		void EncObj(Enc& enc, EAddrOnly) const;
		void EncObj(Enc& enc, EAddrPort) const;

		uint   GetIpLen () const;
		uint32 GetIp4Nr () const;
		Seq    GetIpBin () const;
		uint64 GetIp6Hi () const;
		uint16 GetPort  () const;

		bool IsLocal() const;
		bool IsIpMaxValue() const;

		bool operator<  (SockAddr const& x) const;
		bool operator== (SockAddr const& x) const;

		bool operator!= (SockAddr const& x) const { return !operator==(x); }

	private:
		static SockAddr const* SetIp6FromIp4_IfNotAlreadyIp6(SockAddr const& orig, SockAddr& storage);
	};


	class Socket : NoCopy
	{
	public:
		struct Err : public StrErr
		{
			Err(Seq msg) : StrErr(msg) {};
		};
	
		Socket() {}
		Socket(Socket&& x) noexcept;

		~Socket();

		void Create(int af, int type, int protocol);
		void SetSocket(SOCKET s);
		void Close();

		bool IsValid() const { return m_s != INVALID_SOCKET; }
		SOCKET GetSocket() { return m_s; }
		SOCKET ReleaseSocket() { SOCKET s = m_s; m_s = INVALID_SOCKET; return s; }

		SockAddr const& LocalAddr() const { if (!m_localSa.Valid()) InitLocalAddr(); return m_localSa; }
 		SockAddr const& RemoteAddr() const { if (!m_remoteSa.Valid()) InitRemoteAddr(); return m_remoteSa; }

		void Bind(SockAddr const& sa);
		void SetSockOpt(int level, int optname, int val);	
		void SetNoDelay(bool value) { SetSockOpt(IPPROTO_TCP, TCP_NODELAY, value); }
		void SetRecvTimeout(int seconds) { SetSockOpt(SOL_SOCKET, SO_RCVTIMEO, seconds*1000); }
		void SetSendTimeout(int seconds) { SetSockOpt(SOL_SOCKET, SO_SNDTIMEO, seconds*1000); }
		void SetIpv6Only(bool value) { SetSockOpt(IPPROTO_IPV6, IPV6_V6ONLY, value); }
	
		void Listen(SockAddr const& sa, int backlog = SOMAXCONN);
		Event& AcceptEvent() { return m_acceptEvent; }
		void Accept(Socket& newSk, SockAddr& remoteSa);
	
	private:
		SOCKET m_s { INVALID_SOCKET };
		mutable SockAddr m_localSa;
		mutable SockAddr m_remoteSa;

		bool m_listening {};
		Event m_acceptEvent;

		void InitLocalAddr() const;
		void InitRemoteAddr() const;
	};

}
