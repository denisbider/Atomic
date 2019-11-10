// This file is Copyright (C) 2017-2019 by denis bider. Terms of use are as follows:
//
// The author permits use of this file free of charge, for any lawful purpose.
// No warranty is given or implied. Attribution is NOT required in compiled
// programs. The source code of copies and derivative works MUST reproduce,
// unmodified, this copyright notice, terms of use, and any change descriptions.
// Changes in derivative works are Copyright (C) by their respective authors.
// Authors of changes MUST describe all changes accurately below this notice.
// Changes to line endings that preserve file meaning MAY be excluded.

#pragma once

#ifndef IPTOCOUNTRY_HEADERS_INCLUDED
#include <Windows.h>
#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#define IPTOCOUNTRY_HEADERS_INCLUDED
#endif

class IpToCountry
{
public:
	struct Err : std::exception
	{
		Err(char const* desc, DWORD winErrCode);

		char const* m_desc;
		DWORD m_winErrCode;		// Zero if not present

		char const* what() const override final { return m_desc; }
	};

	struct AlreadyInitialized      : Err { AlreadyInitialized      () : Err("IpToCountry: Already initialized",                  0) {} };
	struct NotInitialized          : Err { NotInitialized          () : Err("IpToCountry: Not initialized",                      0) {} };
	struct PrematureEndOfFile      : Err { PrematureEndOfFile      () : Err("IpToCountry: Premature end of file",                0) {} };
	struct InvalidHeaderSignature  : Err { InvalidHeaderSignature  () : Err("IpToCountry: Invalid header signature",             0) {} };
	struct InvalidNrCountriesBytes : Err { InvalidNrCountriesBytes () : Err("IpToCountry: Invalid number of countries bytes",    0) {} };
	struct InvalidNrIpSectionBytes : Err { InvalidNrIpSectionBytes () : Err("IpToCountry: Invalid number of IP section bytes",   0) {} };
	struct PrematureEndOfData      : Err { PrematureEndOfData      () : Err("IpToCountry: Premature end of data",                0) {} };
	struct EndOfDataExpected       : Err { EndOfDataExpected       () : Err("IpToCountry: End of data expected",                 0) {} };
	struct InvalidNrContinents     : Err { InvalidNrContinents     () : Err("IpToCountry: Invalid number of continents",         0) {} };
	struct InvalidNrCountries      : Err { InvalidNrCountries      () : Err("IpToCountry: Invalid number of countries",          0) {} };
	struct InvalidNrDictEntries    : Err { InvalidNrDictEntries    () : Err("IpToCountry: Invalid number of dictionary entries", 0) {} };
	struct InvalidNrBlocks         : Err { InvalidNrBlocks         () : Err("IpToCountry: Invalid number of IP blocks",          0) {} };
	struct InvalidContinentIndex   : Err { InvalidContinentIndex   () : Err("IpToCountry: Invalid continent index",              0) {} };
	struct InvalidCountryIndex     : Err { InvalidCountryIndex     () : Err("IpToCountry: Invalid country index",                0) {} };
	struct InvalidDictionaryIndex  : Err { InvalidDictionaryIndex  () : Err("IpToCountry: Invalid dictionary index",             0) {} };
	struct InvalidIpAddressLen     : Err { InvalidIpAddressLen     () : Err("IpToCountry: Invalid IP address length",            0) {} };
	struct InvalidNrSigBits        : Err { InvalidNrSigBits        () : Err("IpToCountry: Invalid number of significant bits",   0) {} };
	struct InvalidIpEncoding       : Err { InvalidIpEncoding       () : Err("IpToCountry: Invalid IP address encoding",          0) {} };
	struct InvalidIpBlockOrder     : Err { InvalidIpBlockOrder     () : Err("IpToCountry: Invalid IP block order",               0) {} };
	struct InvalidSubNetEncoding   : Err { InvalidSubNetEncoding   () : Err("IpToCountry: Invalid subnet encoding",              0) {} };

	struct CreateFileFailed     : Err { CreateFileFailed     (DWORD winErrCode) : Err("IpToCountry: CreateFile failed",     winErrCode) {} };
	struct ReadFileFailed       : Err { ReadFileFailed       (DWORD winErrCode) : Err("IpToCountry: ReadFile failed",       winErrCode) {} };
	struct SetFilePointerFailed : Err { SetFilePointerFailed (DWORD winErrCode) : Err("IpToCountry: SetFilePointer failed", winErrCode) {} };

public:
	struct Continent
	{
		unsigned char const* m_code;		// Always two characters. NOT zero-terminated
		unsigned int         m_nameLen;
		unsigned char const* m_name;		// NOT zero-terminated

		bool Known() const { return m_code[0] != '-'; }
	};

	struct Country
	{
		Continent const*     m_continent;
		unsigned char const* m_code;		// Always two characters. NOT zero-terminated
		unsigned int         m_nameLen;
		unsigned char const* m_name;		// NOT zero-terminated

		bool Known() const { return m_code[0] != '-'; }
	};

public:
	// Functions throw IpToCountry::Err on error.

	~IpToCountry();

	// Open() is expected to be called at initialization, and is NOT thread-safe.
	// Other methods for use after Open ARE thread-safe.

	void Open(wchar_t const* fileName);

	std::vector<Continent> const& Continents () const;
	std::vector<Country>   const& Countries  () const;

	// - An address may be found that is not associated with any location. In this case, a non-null
	//   Country will be returned where Country::Known() and Continent::Known() both return false.
	// - An address might not be found at all. In this case, a null pointer will be returned.
	// - The IPv4 and IPv6 blocks aren't read until needed. This allows an application to avoid
	//   the memory overhead of loading e.g. IPv6 data when only IPv4 is needed, or vice versa.
	//   Using the MaxMind GeoLite2 Country database from February 2017, the memory overhead is
	//   about 2 MB for IPv4 blocks, and 1 MB for IPv6.

	Country const* FindCountryByIp4(unsigned char const* ip);
	Country const* FindCountryByIp6(unsigned char const* ip);

	Country const* FindCountryByIp(unsigned char const* ip, size_t ipBytes);

private:
	class Reader
	{
	public:
		Reader(unsigned char const* pRead, size_t n                  ) : m_pRead(pRead), m_pEnd(pRead + n) {}
		Reader(unsigned char const* pRead, unsigned char const* pEnd ) : m_pRead(pRead), m_pEnd(pEnd)      {}

		unsigned char        ReadByte();
		unsigned int         ReadUInt32();
		unsigned char const* ReadVerbatim(size_t n);
		void                 ExpectEnd() const;

	private:
		unsigned char const* m_pRead;
		unsigned char const* m_pEnd;
	};

	HANDLE m_hFile          { INVALID_HANDLE_VALUE };
	bool   m_initStarted    {};
	bool   m_initialized    {};
	bool   m_ip4SectionRead {};
	bool   m_ip6SectionRead {};

	enum { CallOnceThreshold = 0x40000000 };
	unsigned int volatile m_ip4CallOnceState {};
	unsigned int volatile m_ip6CallOnceState {};

	enum
	{
		HeaderBytes          = 16,

		MinCountriesBytes    = 2,
		MaxCountriesBytes    = (1 + (255 * (    2 + 1 + 255))) +
		                       (1 + (255 * (1 + 2 + 1 + 255))),

		MinIpSectionBytes    = 8,
		MaxIpSectionBytes    = 4 * 1000 * 1000,

		MinDictionaryEntries = 1,
		MaxDictionaryEntries = 128,
	};

	unsigned int               m_countriesBytes {};
	unsigned int               m_ip4Bytes {};
	unsigned int               m_ip6Bytes {};

	std::vector<unsigned char> m_countriesBuf;
	std::vector<Continent>     m_continents;
	std::vector<Country>       m_countries;

	void ReadFromFile(void* p, unsigned int n);
	void ReadHeader();
	void ReadCountries();

	struct DictEntry
	{
		byte m_sigBits;
		byte m_countryIndex;
	};

	template <int IpBytes>
	struct IpBlock
	{
		unsigned char m_ip[IpBytes];
		byte          m_sigBits;
		byte          m_countryIndex;
	};

	std::vector<IpBlock< 4>> m_ip4Blocks;
	std::vector<IpBlock<16>> m_ip6Blocks;

	template <int IpBytes>
	void ReadIpSection(std::vector<IpBlock<IpBytes>>& ipBlocks, LONG sectionOffset, unsigned int sectionBytes);

	template <int IpBytes>
	Country const* FindCountryByIpImpl(std::vector<IpBlock<IpBytes>> const& ipBlocks, unsigned char const* ip) const;

	static void SetIpToStartOfSubNet (unsigned char* ip, unsigned int ipBytes, unsigned int sigBits);
	static void SetIpToEndOfSubNet   (unsigned char* ip, unsigned int ipBytes, unsigned int sigBits);
	static void IncrementIp          (unsigned char* ip, unsigned int ipBytes);

	static void CallOnce(unsigned int volatile& callOnceState, std::function<void()> callee);
};
