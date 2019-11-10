#include "GctIncludes.h"
#include "GctMain.h"


Seq IpData::ExtractIpPartToEncode(SockAddr const& expectIp, SockAddr const& ip, sizet sigBits, sizet& nrCommonPrefixBytes, sizet& nrTrailingZeroBytes)
{
	sizet const ipLen = ip.GetIpLen();
	EnsureThrow(ipLen == expectIp.GetIpLen());

	Seq ipBin = ip.GetIpBin();

	nrCommonPrefixBytes = expectIp.GetIpBin().CommonPrefixExact(ipBin).n;
	if (nrCommonPrefixBytes > ipLen)
		throw StrErr(Str("Unexpected number of common prefix bytes: ").UInt(nrCommonPrefixBytes));
	if (nrCommonPrefixBytes > 7)
		nrCommonPrefixBytes = 7;

	nrTrailingZeroBytes = ipLen - ((sigBits + 7) / 8);
	if (nrCommonPrefixBytes + nrTrailingZeroBytes > ipLen)
	{
		nrTrailingZeroBytes = ipLen - nrCommonPrefixBytes;
		return Seq();
	}
	else
	{
		ipBin.DropBytes(nrCommonPrefixBytes);

		EnsureThrow(ipBin.n >= nrTrailingZeroBytes);
		ipBin.n -= nrTrailingZeroBytes;

		while (ipBin.EndsWithByte(0))
		{
			++nrTrailingZeroBytes;
			--(ipBin.n);
		}

		return ipBin;
	}
}


void IpData::Analyze()
{
	sizet commonPrefixDistribution[17] {};

	SockAddr expectIp;
	if (m_ipVer == 4)
		expectIp.SetIp4(0, 0);
	else
		expectIp.SetIp6Any(0, 0);

	for (IpToGeoNameId const& ip : m_ips)
	{
		if (ip.m_ipFirst != expectIp)
		{
			++m_nrHoles;

			sizet nrCommonPrefixBytes, nrTrailingZeroBytes;
			ExtractIpPartToEncode(expectIp, ip.m_ipFirst, ip.m_sigBits, nrCommonPrefixBytes, nrTrailingZeroBytes);

			m_totalTrailingZeroBytes += nrTrailingZeroBytes;
			m_totalCommonPrefixBytes += nrCommonPrefixBytes;
			++(commonPrefixDistribution[nrCommonPrefixBytes]);
		}
		else
		{
			Map<ContOpportunity>::It it = m_contOpportunities.Find(ContOpportunity::MakeKey(ip.m_sigBits, ip.m_geoNameId));
			if (it.Any())
				++(it->m_nrRefs);
			else
			{
				ContOpportunity opp;
				opp.m_sigBits = ip.m_sigBits;
				opp.m_geoNameId = ip.m_geoNameId;
				opp.m_nrRefs = 1;
				m_contOpportunities.Add(std::move(opp));
			}
		}

		expectIp.SetToEndOfSubNet(ip.m_ipFirst, ip.m_sigBits);
		expectIp.IncrementIp();
	}

	Console::Out(Str("Found ").UInt(m_nrHoles).Add(" holes between IPv").SInt(m_ipVer).Add(" address blocks\r\n"));
	Console::Out(Str().UInt(m_totalTrailingZeroBytes).Add(" total trailing zero bytes\r\n"));
	Console::Out(Str().UInt(m_totalCommonPrefixBytes).Add(" total common prefix bytes\r\n"));

	uint maxDistIndex;
	for (maxDistIndex=16; maxDistIndex; --maxDistIndex)
		if (commonPrefixDistribution[maxDistIndex] != 0)
			break;

	Str desc = "Common prefix distribution: ";
	for (uint i=0; i<=maxDistIndex; ++i)
		desc.UInt(i).Add(":").UInt(commonPrefixDistribution[i]).Add(" ");
	desc.Add("\r\n");
	Console::Out(desc);

	for (ContOpportunity const& opp : m_contOpportunities)
	{
		NrRefsToContOpp x;
		x.m_nrRefs = opp.m_nrRefs;
		x.m_contOppKey = opp.Key();
		m_nrRefsToContOpps.Add(std::move(x));
	}

	sizet oppIndex {};
	for (NrRefsToContOpp const& x : m_nrRefsToContOpps)
	{
		m_nrContOppRefs += x.m_nrRefs;

		if (oppIndex <= 127)
		{
			m_nrContOppRefs128 += x.m_nrRefs;

			Map<ContOpportunity>::It oppIt = m_contOpportunities.Find(x.m_contOppKey);
			oppIt->m_dictIndex = (uint) oppIndex;
		}

		++oppIndex;
	}

	Console::Out(Str("Found ").UInt(oppIndex).Add(" distinct types of continuation opportunities\r\n"));
	Console::Out(Str("Found ").UInt(m_nrContOppRefs).Add(" references to any continuation opportunity\r\n"));
	Console::Out(Str("Found ").UInt(m_nrContOppRefs128).Add(" references to top 128 continuation opportunities\r\n"));

	sizet holesAndRefs = m_nrHoles + m_nrContOppRefs;
	if (holesAndRefs == m_ips.Len())
		Console::Out(Str("Nr holes + references == ").UInt(holesAndRefs).Add(": matches nr IP blocks\r\n"));
	else
		Console::Out(Str("Nr holes + references == ").UInt(holesAndRefs).Add(": MISMATCH to nr IP blocks\r\n"));

	Console::Out("\r\n");
}


void GctData::CountNrRefsToGeoNameIds()
{
	uint index = 0;
	for (Continent& c : m_continents)
		c.m_index = index++;

	uint32 maxNrRefs = 0, minNrRefs = UINT32_MAX;
	Seq countryMaxRefs, countryMinRefs;

	index = 0;
	for (GeoNameIdToCountry& x : m_geoNameIds)
	{
		x.m_countryIndex = index++;

		NrRefsToGeoNameId n;
		n.m_geoNameId = x.m_geoNameId;
		n.m_nrRefs = x.m_nrRefs;
		m_nrRefsToGeoNameIds.Add(std::move(n));

		if (maxNrRefs < x.m_nrRefs) { maxNrRefs = x.m_nrRefs; countryMaxRefs = x.m_countryName; }
		if (minNrRefs > x.m_nrRefs) { minNrRefs = x.m_nrRefs; countryMinRefs = x.m_countryName; }
	}

	Console::Out(Str("Country with max refs: ").UInt(maxNrRefs).Add(", ").Add(countryMaxRefs).Add("\r\n"));
	Console::Out(Str("Country with min refs: ").UInt(minNrRefs).Add(", ").Add(countryMinRefs).Add("\r\n"));
	Console::Out("\r\n");
}


void GctData::Analyze()
{
	CountNrRefsToGeoNameIds();
	m_ip4.Analyze();
	m_ip6.Analyze();
}
