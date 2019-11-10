#include "GctIncludes.h"
#include "GctMain.h"


void GctData::ReadCountries()
{
	Str filePath = JoinPath(m_opts.m_inDir.Ref(), "GeoLite2-Country-Locations-en.csv");
	Console::Out(Str("Reading ").Add(filePath).Add("\r\n"));

	FileLoader fileLoader { filePath };
	CsvReader  reader     { fileLoader, ',', 0 };
	reader.SkipLines(1);

	Vec<Str> countryCodesRead;
	countryCodesRead.ReserveExact(300);

	{
		Continent c;
		c.m_code = "--";
		c.m_name = "[unknown]";
		m_continents.Add(std::move(c));

		GeoNameIdToCountry x;
		x.m_geoNameId = 0;
		x.m_continentCode = "--";
		x.m_countryIsoCode = "--";
		x.m_countryName = "[unknown]";
		m_geoNameIds.Add(std::move(x));

		countryCodesRead.Add("--");
	}

	Vec<Str> fields;
	sizet nrFields;
	while (reader.ReadRecord(fields, nrFields))
	{
		if (nrFields >= 4)
		{
			GeoNameIdToCountry x;

			x.m_geoNameId = Seq(fields[0]).ReadNrUInt32Dec();

			if (x.m_geoNameId == 0 || x.m_geoNameId == UINT32_MAX)
				throw StrErr(Str("Invalid geoNameId at line ").UInt(reader.LineNr()).Add(": ").Add(fields[0]));
			if (m_geoNameIds.Find(x.m_geoNameId).Any())
				throw StrErr(Str("Duplicate geoNameId at line ").UInt(reader.LineNr()).Add(": ").Add(fields[0]));

			x.m_continentCode = fields[2];

			if (x.m_continentCode.Len() != 2)
				throw StrErr(Str("Unexpected continent code length: ").Add(x.m_continentCode));

			Map<Continent>::ConstIt it = m_continents.Find(x.m_continentCode);
			if (it.Any())
			{
				if (!Seq(it->m_name).EqualExact(fields[3]))
					throw StrErr(Str("Continent name: \"").Add(fields[3]).Add("\" differs from previous record: \"").Add(it->m_name).Add("\"\r\n"));
			}
			else
			{
				Continent c;
				c.m_code = x.m_continentCode;
				c.m_name = fields[3];

				Console::Out(Str("Found continent ").Add(c.m_code).Add(" - ").Add(c.m_name).Add("\r\n"));
				m_continents.Add(std::move(c));
			}

			x.m_countryIsoCode = fields[4];
			x.m_countryName    = fields[5];

			if (!x.m_countryIsoCode.Any())
			{
				if (x.m_countryName.Any())
					throw StrErr(Str("Unexpected country without ISO code: ").Add(x.m_countryName));

				x.m_countryIsoCode = "--";
				x.m_countryName = "<unknown>";
			}
			else
			{
				if (x.m_countryIsoCode.Len() != 2)
					throw StrErr(Str("Unexpected country ISO code length: ").Add(x.m_countryIsoCode));
				if (countryCodesRead.Contains(x.m_countryIsoCode))
					throw StrErr(Str("Country ISO code ").Add(x.m_countryIsoCode).Add(" conflicts with another country\r\n"));

				countryCodesRead.Add(x.m_countryIsoCode);
			}

			m_geoNameIds.Add(std::move(x));
		}
	}

	Console::Out(Str("Read ").UInt(m_geoNameIds.Len()).Add(" locations, ").UInt(countryCodesRead.Len()).Add(" country codes\r\n"));
	Console::Out("\r\n");
}


void GctData::ReadIps(Seq fileName, IpData& ipData)
{
	Str filePath = JoinPath(m_opts.m_inDir.Ref(), fileName);
	Console::Out(Str("Reading ").Add(filePath).Add("\r\n"));

	FileLoader fileLoader { filePath };
	CsvReader  reader     { fileLoader, ',', 0 };
	reader.SkipLines(1);

	Vec<Str> fields;
	sizet nrFields;
	while (reader.ReadRecord(fields, nrFields))
	{
		if (nrFields >= 2)
		{
			Seq ipReader { fields[0] };
			Seq ipStr = ipReader.ReadToByte('/');

			IpToGeoNameId x;

			if (ipData.m_ipVer == 4)
				x.m_ipFirst.ParseIp4(WinStr(ipStr).Z());
			else
				x.m_ipFirst.ParseIp6(WinStr(ipStr).Z());

			if (!x.m_ipFirst.Valid())
				throw StrErr(Str("Invalid IP address at line ").UInt(reader.LineNr()).Add(": ").Add(ipStr));
			if (x.m_ipFirst.IpVer() != ipData.m_ipVer)
				throw StrErr(Str("Unexpected IP address type at line ").UInt(reader.LineNr()).Add(": ").Add(ipStr));

			x.m_sigBits = ipReader.DropByte().ReadNrUInt16Dec();
			if (x.m_sigBits == 0 || x.m_sigBits == UINT16_MAX)
				throw StrErr(Str("Invalid sigBits at line ").UInt(reader.LineNr()).Add(": ").Add(fields[0]));

			if (!fields[1].Any())
				x.m_geoNameId = 0;
			else
			{
				x.m_geoNameId = Seq(fields[1]).ReadNrUInt32Dec();
				if (x.m_geoNameId == 0 || x.m_geoNameId == UINT32_MAX)
					throw StrErr(Str("Invalid geoNameId at line ").UInt(reader.LineNr()).Add(": ").Add(fields[1]));

				Map<GeoNameIdToCountry>::It it = m_geoNameIds.Find(x.m_geoNameId);
				if (!it.Any())
					throw StrErr(Str("Unknown geoNameId at line ").UInt(reader.LineNr()).Add(": ").Add(fields[1]));
				++(it->m_nrRefs);
			}

			ipData.m_ips.Add(std::move(x));
		}
	}

	Console::Out(Str("Read ").UInt(ipData.m_ips.Len()).Add(" IPv").SInt(ipData.m_ipVer).Add(" address blocks\r\n"));
	Console::Out("\r\n");
}


void GctData::Read()
{
	ReadCountries();
	ReadIps("GeoLite2-Country-Blocks-IPv4.csv", m_ip4);
	ReadIps("GeoLite2-Country-Blocks-IPv6.csv", m_ip6);
}
