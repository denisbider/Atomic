#include "GctIncludes.h"
#include "GctMain.h"


void IpData::CalcWriteBytes()
{
	m_nrDictionaryEntries = (byte) PickMin<sizet>(m_contOpportunities.Len(), 128);
	m_writeBytes = 1U + (2U * m_nrDictionaryEntries) + 4U;

	sizet ipSize = ((m_ipVer == 4) ? 4U : 16U);
	sizet fullBlocksBytes = m_nrHoles * (4U + ipSize);
	sizet notEncodedBytes = m_totalCommonPrefixBytes + m_totalTrailingZeroBytes;
	EnsureThrow(fullBlocksBytes > notEncodedBytes);
	m_writeBytes += fullBlocksBytes - notEncodedBytes;

	m_writeBytes += m_nrContOppRefs128;

	m_writeBytes += 2U * (m_nrContOppRefs - m_nrContOppRefs128);
}


void IpData::WriteDictionary(Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds)
{
	EncodeByte(enc, m_nrDictionaryEntries);

	uint index = 0;
	for (NrRefsToContOpp const& n : m_nrRefsToContOpps)
	{
		Map<ContOpportunity>::ConstIt contOpp = m_contOpportunities.Find(n.m_contOppKey);
		Map<GeoNameIdToCountry>::ConstIt geoNameIt = geoNameIds.Find(contOpp->m_geoNameId);
		
		EncodeByte(enc, NumCast<byte>(contOpp->m_sigBits));
		EncodeByte(enc, NumCast<byte>(geoNameIt->m_countryIndex));

		if (++index == m_nrDictionaryEntries)
			break;
	}
}


void IpData::WriteBlocks(Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds)
{
	EncodeUInt32(enc, NumCast<uint32>(m_ips.Len()));

	SockAddr expectIp;
	if (m_ipVer == 4)
		expectIp.SetIp4(0, 0);
	else
		expectIp.SetIp6Any(0, 0);

	for (IpToGeoNameId const& ip : m_ips)
	{
		if (ip.m_ipFirst != expectIp)
		{
			// Block-Start
			Map<GeoNameIdToCountry>::ConstIt geoNameIt = geoNameIds.Find(ip.m_geoNameId);

			EnsureThrow(ip.m_sigBits >= 1 && ip.m_sigBits <= 128);
			EncodeByte(enc, (byte) (0x80U | (ip.m_sigBits - 1U)));
			
			EncodeByte(enc, 255);

			EnsureThrow(geoNameIt->m_countryIndex <= 254);
			EncodeByte(enc, (byte) geoNameIt->m_countryIndex);

			sizet nrCommonPrefixBytes, nrTrailingZeroBytes;
			Seq encodeIp = ExtractIpPartToEncode(expectIp, ip.m_ipFirst, ip.m_sigBits, nrCommonPrefixBytes, nrTrailingZeroBytes);

			EnsureThrow(nrCommonPrefixBytes <= 7);
			EnsureThrow(encodeIp.n <= 16);
			EncodeByte(enc, (byte) ((nrCommonPrefixBytes << 5) | encodeIp.n));
			EncodeVerbatim(enc, encodeIp);
		}
		else
		{
			// Block-Continuation-..
			Map<ContOpportunity>::ConstIt oppIt = m_contOpportunities.Find(ContOpportunity::MakeKey(ip.m_sigBits, ip.m_geoNameId));
			if (oppIt->m_dictIndex <= 127)
			{
				// Block-Continuation-Dict
				EncodeByte(enc, (byte) (oppIt->m_dictIndex));
			}
			else
			{
				// Block-Continuation-Explicit
				Map<GeoNameIdToCountry>::ConstIt geoNameIt = geoNameIds.Find(ip.m_geoNameId);

				EnsureThrow(ip.m_sigBits >= 1 && ip.m_sigBits <= 128);
				EncodeByte(enc, (byte) (0x80U | (ip.m_sigBits - 1U)));

				EnsureThrow(geoNameIt->m_countryIndex <= 254);
				EncodeByte(enc, (byte) geoNameIt->m_countryIndex);
			}
		}

		expectIp.SetToEndOfSubNet(ip.m_ipFirst, ip.m_sigBits);
		expectIp.IncrementIp();
	}
}


void IpData::Write(Enc& enc, Map<GeoNameIdToCountry> const& geoNameIds)
{
	Enc::Meter meter = enc.IncMeter(m_writeBytes);
	WriteDictionary(enc, geoNameIds);
	WriteBlocks(enc, geoNameIds);
	EnsureThrow(meter.Met());
}


void GctData::CalcCountriesBytes()
{
	m_countriesBytes = 1;
	for (Continent const& c : m_continents)
		m_countriesBytes += 2 + 1 + c.m_name.Len();

	m_countriesBytes += 1;
	for (GeoNameIdToCountry const& g : m_geoNameIds)
		m_countriesBytes += 1 + 2 + 1 + g.m_countryName.Len();
}


void GctData::WriteHeader(Enc& enc)
{
	Enc::Meter meter = enc.IncMeter(HeaderSize);
	EncodeVerbatim(enc, "GCT1");
	EncodeUInt32(enc, NumCast<uint32>(m_countriesBytes));
	EncodeUInt32(enc, NumCast<uint32>(m_ip4.m_writeBytes));
	EncodeUInt32(enc, NumCast<uint32>(m_ip6.m_writeBytes));
	EnsureThrow(meter.Met());
}


void GctData::WriteCountries(Enc& enc)
{
	Enc::Meter meter = enc.IncMeter(m_countriesBytes);

	if (m_continents.Len() > 255) throw StrErr("Current write format cannot accommodate more than 255 continents");
	EncodeByte(enc, NumCast<byte>(m_continents.Len()));
	for (Continent const& c : m_continents)
	{
		if (c.m_code.Len() !=  2) throw StrErr(Str("Unexpected continent code length: ").Add(c.m_code));
		if (c.m_name.Len() > 255) throw StrErr(Str("Unexpected continent name length: ").Add(c.m_name));

		EncodeVerbatim (enc, c.m_code);
		EncodeByte     (enc, NumCast<byte>(c.m_name.Len()));
		EncodeVerbatim (enc, c.m_name);
	}

	if (m_geoNameIds.Len() > 255) throw StrErr("Current write format cannot accommodate more than 255 countries");
	EncodeByte(enc, NumCast<byte>(m_geoNameIds.Len()));
	for (GeoNameIdToCountry const& g : m_geoNameIds)
	{
		Map<Continent>::ConstIt continentIt = m_continents.Find(g.m_continentCode);
		EncodeByte(enc, NumCast<byte>(continentIt->m_index));

		if (g.m_countryIsoCode.Len() !=  2) throw StrErr(Str("Unexpected country code length: ").Add(g.m_countryIsoCode));
		if (g.m_countryName.Len()    > 255) throw StrErr(Str("Unexpected country name length: ").Add(g.m_countryName));

		EncodeVerbatim (enc, g.m_countryIsoCode);
		EncodeByte     (enc, NumCast<byte>(g.m_countryName.Len()));
		EncodeVerbatim (enc, g.m_countryName);
	}

	EnsureThrow(meter.Met());
}


void GctData::Write()
{
	CalcCountriesBytes();
	m_ip4.CalcWriteBytes();
	m_ip6.CalcWriteBytes();

	sizet bytesToWrite = HeaderSize + m_countriesBytes + m_ip4.m_writeBytes + m_ip6.m_writeBytes;
	Console::Out(Str("Encoding ").UInt(bytesToWrite).Add(" bytes\r\n"));

	Str buf;
	Enc::Meter meter = buf.IncMeter(bytesToWrite);

	WriteHeader(buf);
	WriteCountries(buf);
	m_ip4.Write(buf, m_geoNameIds);
	m_ip6.Write(buf, m_geoNameIds);
	EnsureThrow(meter.Met());

	if (m_opts.m_outDir.Any())
	{
		Str outFilePath = JoinPath(m_opts.m_outDir.Ref(), "Countries.bin");
		Console::Out(Str("Writing to ").Add(outFilePath).Add("\r\n"));

		File outFile;
		outFile.Open(outFilePath, File::OpenArgs::DefaultOverwrite());
		outFile.Write(buf);
	}
}
