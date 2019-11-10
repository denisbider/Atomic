#pragma once

#include "GctIncludes.h"


struct Opts
{
	Opt<Str> m_inDir;
	Opt<Str> m_outDir;
};


struct Continent
{
	Str  m_code;
	Str  m_name;

	uint m_index { UINT_MAX };

	Str const& Key() const { return m_code; }
};


struct GeoNameIdToCountry
{
	uint32 m_geoNameId {};
	Str    m_continentCode;
	Str    m_countryIsoCode;
	Str    m_countryName;

	uint   m_countryIndex { UINT_MAX };

	uint32 m_nrRefs {};

	uint32 Key() const { return m_geoNameId; }
};


struct IpToGeoNameId
{
	SockAddr m_ipFirst;
	uint16   m_sigBits   {};
	uint32   m_geoNameId {};		// 0 indicates no associated location

	SockAddr const& Key() const { return m_ipFirst; }
};


struct NrRefsToGeoNameId
{
	uint32 m_nrRefs     {};
	uint32 m_geoNameId  {};

	uint32 Key() const { return m_nrRefs; }
};


struct ContOpportunity
{
	uint16 m_sigBits   {};
	uint32 m_geoNameId {};
	uint32 m_nrRefs    {};

	uint   m_dictIndex { UINT_MAX };

	static uint64 MakeKey(uint sigBits, uint32 geoNameId) { return (((uint64) sigBits) << 32) | geoNameId; }
	uint64 Key() const { return MakeKey(m_sigBits, m_geoNameId); }
};


struct NrRefsToContOpp
{
	uint32 m_nrRefs     {};
	uint64 m_contOppKey {};

	uint32 Key() const { return m_nrRefs; }
};


struct IpData
{
	IpData(uint ipVer) : m_ipVer(ipVer) {}

	uint const              m_ipVer;
	Map<IpToGeoNameId>      m_ips;
	Map<ContOpportunity>    m_contOpportunities;
	MapR<NrRefsToContOpp>   m_nrRefsToContOpps;

	uint32 m_nrHoles {};
	sizet  m_totalTrailingZeroBytes {};
	sizet  m_totalCommonPrefixBytes {};
	sizet  m_nrContOppRefs {};
	sizet  m_nrContOppRefs128 {};

	static Seq ExtractIpPartToEncode(SockAddr const& expectIp, SockAddr const& ip, sizet sigBits, sizet& nrCommonPrefixBytes, sizet& nrTrailingZeroBytes);

	void Analyze();

	byte  m_nrDictionaryEntries {};
	sizet m_writeBytes {};

	void CalcWriteBytes  ();
	void WriteDictionary (Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds);
	void WriteBlocks     (Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds);
	void Write           (Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds);
};


struct GctData
{
	GctData(Opts const& opts) : m_opts(opts) {}

	Opts const&             m_opts;
	Map<Continent>          m_continents;
	Map<GeoNameIdToCountry> m_geoNameIds;
	MapR<NrRefsToGeoNameId> m_nrRefsToGeoNameIds;
	IpData                  m_ip4 { 4 };
	IpData                  m_ip6 { 6 };

	void ReadCountries();
	void ReadIps(Seq fileName, IpData& ipData);
	void Read();

	void CountNrRefsToGeoNameIds();
	void Analyze();

	sizet m_countriesBytes {};

	enum { HeaderSize = 16 };

	void CalcCountriesBytes();
	void WriteHeader(Enc& enc);
	void WriteCountries(Enc& enc);
	void Write();
	void Validate();
};
