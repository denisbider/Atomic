#include "GctIncludes.h"
#include "GctMain.h"

#define IPTOCOUNTRY_HEADERS_INCLUDED
#include "IpToCountryHdr.h"


void GctData::Validate()
{
	try
	{
		Console::Out("\r\nValidating\r\n");

		Str fileName = JoinPath(m_opts.m_outDir.Ref(), "Countries.bin");

		IpToCountry ip2c;
		ip2c.Open(WinStr(fileName).Z());

		auto findCountry = [&] (Seq ip)
			{
				SockAddr sa;
				if (!sa.Parse(ip).Valid())
					throw StrErr(Str("Error parsing IP: ").Add(ip));

				Seq ipBin = sa.GetIpBin();
				IpToCountry::Country const* c = ip2c.FindCountryByIp(ipBin.p, ipBin.n);

				Str msg = Str::Join(ip, ": ");

				if (!c)
					msg.Add("[null]");
				else
				{
					Seq countryCode { c->m_code, 2 };
					Seq countryName { c->m_name, c->m_nameLen };
					Seq continentCode { c->m_continent->m_code, 2 };
					Seq continentName { c->m_continent->m_name, c->m_continent->m_nameLen };

					msg.Add(countryCode).Add(", ").Add(countryName).Add(", ").Add(continentCode).Add(", ").Add(continentName);
				}

				msg.Add("\r\n");
				Console::Out(msg);
			};

		findCountry("0.0.0.0");
		findCountry("127.0.0.1");
		findCountry("10.31.101.159");
		findCountry("5.145.149.141");
		findCountry("5.145.149.142");
		findCountry("5.145.149.143");
		findCountry("84.39.223.239");
		findCountry("190.10.32.6");
		findCountry("144.232.236.173");
		findCountry("72.14.223.228");
		findCountry("66.249.95.225");
		findCountry("209.85.242.161");
		findCountry("8.8.8.4");
		findCountry("8.8.8.8");
		findCountry("163.172.16.125");
		findCountry("::");
		findCountry("::1");
		findCountry("2001:638:807:204::8d59:e17e");
		findCountry("2001:4860:4860::8844");
		findCountry("2001:4860:4860::8888");
	}
	catch (IpToCountry::Err const& e)
	{
		Str msg = "IpToCountry error: ";
		msg.Add(e.m_desc);

		if (e.m_winErrCode)
			throw WinErr<>(e.m_winErrCode, msg);

		throw StrErr(msg);
	}
}
