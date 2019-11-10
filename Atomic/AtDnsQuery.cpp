#include "AtIncludes.h"
#include "AtDnsQuery.h"

#include "AtException.h"
#include "AtNumCvt.h"
#include "AtWinStr.h"


namespace At
{

	// IpVerPreference

	DESCENUM_DEF_BEGIN(IpVerPreference)
	DESCENUM_DEF_VAL_X(None, "No preference")
	DESCENUM_DEF_VAL_X(Ip4,  "Prefer IPv4")
	DESCENUM_DEF_VAL_X(Ip6,  "Prefer IPv6")
	DESCENUM_DEF_CLOSE();



	// BlockingDnsQuery

	BlockingDnsQuery::~BlockingDnsQuery()
	{
		if (!!m_results)
			DnsRecordListFree(m_results, DnsFreeRecordList);
	}

	DNS_STATUS BlockingDnsQuery::Query(PCWSTR name, WORD type, DWORD options)
	{
		if (!!m_results)
			DnsRecordListFree(m_results, DnsFreeRecordList);
	
		return DnsQuery_W(name, type, options, 0, &m_results, 0);
	}



	// BlockingAddrLookup

	BlockingAddrLookup::~BlockingAddrLookup()
	{
		if (m_result)
			FreeAddrInfoW(m_result);
	}

	int BlockingAddrLookup::Lookup(PCWSTR nodeName, PCWSTR serviceName, ADDRINFOW const* hints)
	{
		if (m_result)
			FreeAddrInfoW(m_result);
		
		return GetAddrInfoW(nodeName, serviceName, hints, &m_result);
	}



	// AsyncAddrLookup

	AsyncAddrLookup::~AsyncAddrLookup()
	{
		if (m_result)
			FreeAddrInfoW(m_result);
	}

	void AsyncAddrLookup::ThreadMain()
	{
		WinStr wsNodeName { m_nodeName }, wsServiceName { m_serviceName };
		m_rc = GetAddrInfoW(
			If(!m_nodeName.Any(),    wchar_t const*, nullptr, wsNodeName.Z()),
			If(!m_serviceName.Any(), wchar_t const*, nullptr, wsServiceName.Z()),
			If(!m_useHints,          PADDRINFOW,     nullptr, &m_hints),
			&m_result);
	}


	
	// LookedUpAddr
	
	void LookedUpAddr::EncObj(Enc& s) const
	{
		if (!m_dnsName.Any())
			s.Obj(m_sa, SockAddr::AddrOnly);
		else
			s.ReserveInc(m_dnsName.Len() + 2 + SockAddr::MaxEncLen + 1)
			 .Add(m_dnsName).Add(" (").Obj(m_sa, SockAddr::AddrOnly).Add(")");
	}


	
	// ProcessAddrInfoResults

	void ProcessAddrInfoResults(PADDRINFOW ai, Seq dnsNameOpt, IpVerPreference::E ipVerPreference, uint sourcePreference, Map<LookedUpAddr>& addrs)
	{
		uint64 sourcePref64 = (((uint64) sourcePreference) << 32);

		for (; ai; ai=ai->ai_next)
		{
			LookedUpAddr addr;
			if (addr.m_sa.Set(*ai).Valid())
			{
				// RFC 5321: "If there are multiple destinations with the same preference and there is no clear reason to
				// favor one (e.g., by recognition of an easily reached address), then the sender-SMTP MUST randomize them
				// to spread the load across multiple mail exchangers for a specific organization."
				//
				// Rather than detect equal preference numbers, we randomize all in a way that preserves original preference order.
				// If an MX name maps to multiple IPv4 / IPv6 address records, we randomize within those, too.

				addr.m_dnsName = dnsNameOpt;

				uint64 ipPref64 = 0;
				if ((addr.m_sa.IsIp4() && ipVerPreference == IpVerPreference::Ip4) ||
					(addr.m_sa.IsIp6() && ipVerPreference == IpVerPreference::Ip6))
				{
					ipPref64 = 1ULL << 48;
				}

				uint32 randomPart {};
				errno_t randErr { rand_s(&randomPart) };
				if (randErr != 0) throw ErrWithCode<>(randErr, __FUNCTION__ ": Error in rand_s");
				addr.m_preference = ipPref64 | sourcePref64 | randomPart;

				addrs.Add(std::move(addr));
			}
		}
	}



	// BlockingMxLookup

	DNS_STATUS BlockingMxLookup::Lookup(Seq domainName, IpVerPreference::E ipVerPreference)
	{
		m_results.Clear();

		// Relevant spec: RFC 5321, section 5.1 - Locating the Target Host

		// Is input an IPv4 or IPv6 address?
		{
			LookedUpAddr mxa;
			if (mxa.m_sa.Parse(domainName).Valid())
			{
				m_results.Add(std::move(mxa));
				return 0;
			}
		}

		// Input is (probably) a DNS name
		WinStr           domainNameW    { domainName };
		BlockingDnsQuery dnsQuery;
		int              firstLookupErr {};
		DNS_STATUS       rc             { dnsQuery.Query(domainNameW.Z(), DNS_TYPE_MX, DNS_QUERY_STANDARD) };

		if (rc == DNS_INFO_NO_RECORDS)
			firstLookupErr = rc;
		else if (rc != 0)
			return rc;

		bool haveMxResults {};
		if (!rc && dnsQuery.m_results)
		{
			for (DNS_RECORDW* rec=dnsQuery.m_results; rec; rec=rec->pNext)
				if (rec->wType == DNS_TYPE_MX)
				{
					haveMxResults = true;
					TryAddMx(rec->Data.MX.pNameExchange, ipVerPreference, rec->Data.MX.wPreference, firstLookupErr);
				}
		}

		if (!haveMxResults)
		{
			// Implicit MX. RFC 5321: "If an empty list of MXs is returned, the address is treated as if it was associated 
			// with an implicit MX RR, with a preference of 0, pointing to that host.  If MX records are present, but none
			// of them are usable, or the implicit MX is unusable, this situation MUST be reported as an error."
			TryAddMx(domainNameW.Z(), ipVerPreference, 0, firstLookupErr);
		}

		if (!m_results.Any())
			return firstLookupErr;
	
		return 0;
	}


	void BlockingMxLookup::TryAddMx(PCWSTR mxNameW, IpVerPreference::E ipVerPreference, uint sourcePreference, int& firstLookupErr)
	{
		ADDRINFOW hints {};
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		BlockingAddrLookup addrInfo;
		int lookupErr { addrInfo.Lookup(mxNameW, L"25", &hints) };
		if (lookupErr != 0)
		{
			if (!firstLookupErr)
				firstLookupErr = lookupErr;
		}
		else
		{
			Str dnsNameStr;
			FromUtf16(mxNameW, NumCast<USHORT>(ZLen(mxNameW)), dnsNameStr, CP_UTF8);
			Seq dnsName = Seq(dnsNameStr).Trim();

			ProcessAddrInfoResults(addrInfo.m_result, dnsName, ipVerPreference, sourcePreference, m_results);
		}
	}

}
