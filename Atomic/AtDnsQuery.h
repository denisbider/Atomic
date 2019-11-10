#pragma once

#include "AtAbortable.h"
#include "AtDescEnum.h"
#include "AtMap.h"
#include "AtSocket.h"
#include "AtThread.h"


namespace At
{
	// IpVerPreference

	DESCENUM_DECL_BEGIN(IpVerPreference)
	DESCENUM_DECL_VALUE(None,         0)
	DESCENUM_DECL_VALUE(Ip4,        100)
	DESCENUM_DECL_VALUE(Ip6,        200)
	DESCENUM_DECL_CLOSE();



	// BlockingDnsQuery

	struct BlockingDnsQuery : NoCopy
	{
		~BlockingDnsQuery();
	
		DNS_STATUS Query(PCWSTR name, WORD type, DWORD options);

		DNS_RECORDW* m_results {};
	};



	// BlockingAddrLookup

	struct BlockingAddrLookup : NoCopy
	{
		~BlockingAddrLookup();	
		int Lookup(PCWSTR nodeName, PCWSTR serviceName, ADDRINFOW const* hints);

		PADDRINFOW m_result {};
	};



	// AsyncAddrLookup

	class AsyncAddrLookup : public Thread
	{
	public:
		~AsyncAddrLookup();

		// Input
		Str        m_nodeName;
		Str        m_serviceName;
		bool       m_useHints {};
		ADDRINFOW  m_hints {};

		// Output
		int        m_rc;
		PADDRINFOW m_result {};

	private:
		void ThreadMain();
	};



	// LookedUpAddr

	struct LookedUpAddr
	{
		uint64   m_preference {};
		Str      m_dnsName;				// Empty if not known
		SockAddr m_sa;

		uint64 Key() const { return m_preference; }
		void EncObj(Enc& s) const;
	};


	
	// ProcessAddrInfoResults

	// Returns GetAddrInfo results ordered by IP version preference and then randomly within preference set.
	// Does NOT clear result vector before adding to it.
	void ProcessAddrInfoResults(PADDRINFOW ai, Seq dnsNameOpt, IpVerPreference::E ipVerPreference, uint sourcePreference, Map<LookedUpAddr>& addrs);



	// BlockingMxLookup

	struct BlockingMxLookup
	{
		DNS_STATUS Lookup(Seq domainName, IpVerPreference::E ipVerPreference);

		Map<LookedUpAddr> m_results;

	private:
		void TryAddMx(PCWSTR mxName, IpVerPreference::E ipVerPreference, uint sourcePreference, int& firstLookupErr);
	};



	// AsyncMxLookup

	class AsyncMxLookup : public Thread
	{
	public:
		// Input
		Str                m_domain;
		IpVerPreference::E m_ipVerPreference { IpVerPreference::None };

		// Output
		BlockingMxLookup   m_mxLookup;
		DNS_STATUS         m_rc;

	private:
		void ThreadMain() { m_rc = m_mxLookup.Lookup(m_domain, m_ipVerPreference); }
	};

}
