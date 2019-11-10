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

#include "IpToCountryHdr.h"


IpToCountry::Err::Err(char const* desc, DWORD winErrCode) : m_desc(desc), m_winErrCode(winErrCode)
{
	if (IsDebuggerPresent())
		DebugBreak();
}


unsigned char IpToCountry::Reader::ReadByte()
{
	if (m_pRead == m_pEnd)
		throw PrematureEndOfData();

	return *m_pRead++;
}


unsigned int IpToCountry::Reader::ReadUInt32()
{
	unsigned int v =
	     (((unsigned int) ReadByte()) << 24);
	v |= (((unsigned int) ReadByte()) << 16);
	v |= (((unsigned int) ReadByte()) <<  8);
	v |=   (unsigned int) ReadByte();
	return v;
}


unsigned char const* IpToCountry::Reader::ReadVerbatim(size_t n)
{
	if (m_pRead + n > m_pEnd)
		throw PrematureEndOfData();

	unsigned char const* p = m_pRead;
	m_pRead += n;
	return p;
}


void IpToCountry::Reader::ExpectEnd() const
{
	if (m_pRead != m_pEnd)
		throw EndOfDataExpected();
}


IpToCountry::~IpToCountry()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}


void IpToCountry::Open(wchar_t const* fileName)
{
	if (m_initStarted)
		throw AlreadyInitialized();

	m_initStarted = true;

	m_hFile = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
	if (m_hFile == INVALID_HANDLE_VALUE)
		throw CreateFileFailed(GetLastError());

	ReadHeader();
	ReadCountries();

	m_initialized = true;
}


std::vector<IpToCountry::Continent> const& IpToCountry::Continents() const
{
	if (!m_initialized)
		throw NotInitialized();

	return m_continents;
}


std::vector<IpToCountry::Country> const& IpToCountry::Countries() const
{
	if (!m_initialized)
		throw NotInitialized();

	return m_countries;
}


IpToCountry::Country const* IpToCountry::FindCountryByIp4(unsigned char const* ip)
{
	if (!m_initialized)
		throw NotInitialized();

	if (!m_ip4SectionRead)
		CallOnce(m_ip4CallOnceState, [this]
			{
				ReadIpSection<4>(m_ip4Blocks, (LONG) (HeaderBytes + m_countriesBytes), m_ip4Bytes);
				m_ip4SectionRead = true;
			} );

	return FindCountryByIpImpl<4>(m_ip4Blocks, ip);
}


IpToCountry::Country const* IpToCountry::FindCountryByIp6(unsigned char const* ip)
{
	if (!m_initialized)
		throw NotInitialized();

	if (!m_ip6SectionRead)
		CallOnce(m_ip6CallOnceState, [this]
			{
				ReadIpSection<16>(m_ip6Blocks, (LONG) (HeaderBytes + m_countriesBytes + m_ip4Bytes), m_ip6Bytes);
				m_ip6SectionRead = true;
			} );

	return FindCountryByIpImpl<16>(m_ip6Blocks, ip);
}


IpToCountry::Country const* IpToCountry::FindCountryByIp(unsigned char const* ip, size_t ipBytes)
{
	if (ipBytes == 4)
		return FindCountryByIp4(ip);

	if (ipBytes == 16)
		return FindCountryByIp6(ip);

	throw InvalidIpAddressLen();
}


void IpToCountry::ReadFromFile(void* p, unsigned int n)
{
	DWORD bytesRead {};
	if (!ReadFile(m_hFile, p, n, &bytesRead, 0))
		throw ReadFileFailed(GetLastError());
	if (bytesRead != n)
		throw PrematureEndOfFile();
}


void IpToCountry::ReadHeader()
{
	unsigned char headerBuf[HeaderBytes] {};
	ReadFromFile(headerBuf, HeaderBytes);

	if (0 != memcmp(headerBuf, "GCT1", 4))
		throw InvalidHeaderSignature();

	Reader reader { headerBuf + 4, headerBuf + HeaderBytes };
	m_countriesBytes = reader.ReadUInt32();
	m_ip4Bytes       = reader.ReadUInt32();
	m_ip6Bytes       = reader.ReadUInt32();
	reader.ExpectEnd();

	if (m_countriesBytes < MinCountriesBytes || m_countriesBytes > MaxCountriesBytes) throw InvalidNrCountriesBytes();
	if (m_ip4Bytes       < MinIpSectionBytes || m_ip4Bytes       > MaxIpSectionBytes) throw InvalidNrIpSectionBytes();
	if (m_ip6Bytes       < MinIpSectionBytes || m_ip6Bytes       > MaxIpSectionBytes) throw InvalidNrIpSectionBytes();
}


void IpToCountry::ReadCountries()
{
	m_countriesBuf.resize(m_countriesBytes);
	ReadFromFile(&m_countriesBuf[0], m_countriesBytes);

	Reader reader { &m_countriesBuf[0], m_countriesBytes };

	unsigned int nrContinents = reader.ReadByte();
	if (nrContinents < 1)
		throw InvalidNrContinents();

	m_continents.reserve(nrContinents);
	for (unsigned int i=0; i!=nrContinents; ++i)
	{
		m_continents.emplace_back();
		Continent& c = m_continents.back();
		c.m_code    = reader.ReadVerbatim(2);
		c.m_nameLen = reader.ReadByte();
		c.m_name    = reader.ReadVerbatim(c.m_nameLen);
	}

	unsigned int nrCountries = reader.ReadByte();
	if (nrCountries < 1)
		throw InvalidNrCountries();

	m_countries.reserve(nrCountries);
	for (unsigned int i=0; i!=nrCountries; ++i)
	{
		m_countries.emplace_back();

		unsigned int continentIndex = reader.ReadByte();
		if (continentIndex >= m_continents.size())
			throw InvalidContinentIndex();

		Country& c = m_countries.back();
		c.m_continent = &m_continents[continentIndex];
		c.m_code      = reader.ReadVerbatim(2);
		c.m_nameLen   = reader.ReadByte();
		c.m_name      = reader.ReadVerbatim(c.m_nameLen);
	}

	reader.ExpectEnd();
}


template <int IpBytes>
void IpToCountry::ReadIpSection(std::vector<IpBlock<IpBytes>>& ipBlocks, LONG sectionOffset, unsigned int sectionBytes)
{
	if (SetFilePointer(m_hFile, sectionOffset, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		throw SetFilePointerFailed(GetLastError());

	std::vector<unsigned char> sectionBuf;
	sectionBuf.resize(sectionBytes);
	ReadFromFile(&sectionBuf[0], sectionBytes);

	Reader reader { &sectionBuf[0], sectionBytes };

	// Dictionary
	unsigned int nrDictionaryEntries = reader.ReadByte();
	if (nrDictionaryEntries < MinDictionaryEntries || nrDictionaryEntries > MaxDictionaryEntries)
		throw InvalidNrDictEntries();

	std::vector<DictEntry> dictionary;
	dictionary.reserve(nrDictionaryEntries);

	for (unsigned int i=0; i!=nrDictionaryEntries; ++i)
	{
		dictionary.emplace_back();
		DictEntry& e = dictionary.back();
		e.m_sigBits      = reader.ReadByte();
		e.m_countryIndex = reader.ReadByte();
	}

	// Blocks
	unsigned int nrBlocks = reader.ReadUInt32();
	if (nrBlocks < 1 || nrBlocks > sectionBytes)
		throw InvalidNrBlocks();

	ipBlocks.reserve(nrBlocks);

	unsigned char ipExpected[IpBytes] {};
	for (unsigned int i=0; i!=nrBlocks; ++i)
	{
		ipBlocks.emplace_back();
		IpBlock<IpBytes>& b = ipBlocks.back();

		unsigned int dictIndex = reader.ReadByte();
		if (dictIndex < 128)
		{
			// Block-Continuation-Dict
			if (dictIndex >= dictionary.size())
				throw InvalidDictionaryIndex();

			DictEntry const& e = dictionary[dictIndex];
			memcpy(b.m_ip, ipExpected, IpBytes);
			b.m_sigBits      = e.m_sigBits;
			b.m_countryIndex = e.m_countryIndex;
		}
		else
		{
			b.m_sigBits = (unsigned char) (((unsigned int) (dictIndex & 0x7F)) + 1);
			b.m_countryIndex = reader.ReadByte();
			if (b.m_countryIndex != 255)
			{
				// Block-Continuation-Explicit
				memcpy(b.m_ip, ipExpected, IpBytes);
			}
			else
			{
				// Block-Start
				b.m_countryIndex = reader.ReadByte();

				unsigned int ipEncodeParams      = reader.ReadByte();
				unsigned int nrCommonPrefixBytes = (ipEncodeParams >> 5);
				unsigned int nrEncodedBytes      = (ipEncodeParams & 0x1F);
				unsigned int nrBytesSet          = nrCommonPrefixBytes + nrEncodedBytes;

				if (nrBytesSet > IpBytes)
					throw InvalidIpEncoding();

				unsigned char const* pEncodedBytes = reader.ReadVerbatim(nrEncodedBytes);
				unsigned char* pWrite = b.m_ip;
				memcpy(pWrite, ipExpected,    nrCommonPrefixBytes ); pWrite += nrCommonPrefixBytes;
				memcpy(pWrite, pEncodedBytes, nrEncodedBytes      ); pWrite += nrEncodedBytes;
				memset(pWrite, 0, IpBytes - nrBytesSet);

				if (memcmp(b.m_ip, ipExpected, IpBytes) <= 0)
					throw InvalidIpBlockOrder();

				memcpy(ipExpected, b.m_ip, IpBytes);
			}
		}

		if (b.m_sigBits < 1 || b.m_sigBits > (8*IpBytes))
			throw InvalidNrSigBits();
		if (b.m_countryIndex >= m_countries.size())
			throw InvalidCountryIndex();

		SetIpToStartOfSubNet(ipExpected, IpBytes, b.m_sigBits);
		if (memcmp(ipExpected, b.m_ip, IpBytes) != 0)
			throw InvalidSubNetEncoding();

		SetIpToEndOfSubNet(ipExpected, IpBytes, b.m_sigBits);
		IncrementIp(ipExpected, IpBytes);
	}

	reader.ExpectEnd();
}


template <int IpBytes>
IpToCountry::Country const* IpToCountry::FindCountryByIpImpl(std::vector<IpBlock<IpBytes>> const& ipBlocks, unsigned char const* ip) const
{
	// std::lower_bound returns the first block that is greater than or equal to ip; otherwise, end() if none such found
	auto it = std::lower_bound(ipBlocks.begin(), ipBlocks.end(), ip,
		[] (IpBlock<IpBytes> const& block, unsigned char const* ip) -> bool
			{ return memcmp(block.m_ip, ip, IpBytes) < 0; } );

	if (it == ipBlocks.end())
	{
		// There is no block that is greater than or equal to ip. Therefore, there must be a last block that is less than ip.
		if (ipBlocks.empty())
			throw InvalidNrBlocks();

		--it;
	}
	else
	{
		// Block found that is greater or equal to ip.
		if (memcmp(it->m_ip, ip, IpBytes) > 0)
		{
			// Block is greater than ip. We are interested in previous block that is less than ip.
			if (it == ipBlocks.begin())
			{
				// There is no block less than or equal to ip. No match. Return null.
				return nullptr;
			}

			--it;
		}
	}

	// We have the highest block that is less than or equal to ip. Construct a copy of ip with insignificant bits cleared.
	unsigned char sigIp[IpBytes];
	memcpy(sigIp, ip, IpBytes);
	SetIpToStartOfSubNet(sigIp, IpBytes, it->m_sigBits);

	// We have a copy of ip with insignificant bits cleared. Does it match the block IP?
	if (memcmp(it->m_ip, sigIp, IpBytes) != 0)
	{
		// No match. Return null.
		return nullptr;
	}

	// This block matches the ip.
	return &m_countries[it->m_countryIndex];
}


void IpToCountry::SetIpToStartOfSubNet(unsigned char* ip, unsigned int ipBytes, unsigned int sigBits)
{
	unsigned int ipBits = 8 * ipBytes;

	if (sigBits > ipBits)
		throw InvalidNrSigBits();

	if (sigBits < ipBits)
	{
		unsigned int partBytePos = (sigBits / 8U);
		unsigned int sigBitsInPartByte = (sigBits % 8U);
		unsigned int bitMask = 0x80;
		unsigned int andMask = 0;
		for (unsigned int k=0; k!=sigBitsInPartByte; ++k)
		{
			andMask |= bitMask;
			bitMask >>= 1;
		}

		ip[partBytePos] &= andMask;
		for (unsigned int k=partBytePos+1; k!=ipBytes; ++k)
			ip[k] = 0;
	}
}


void IpToCountry::SetIpToEndOfSubNet(unsigned char* ip, unsigned int ipBytes, unsigned int sigBits)
{
	unsigned int ipBits = 8 * ipBytes;

	if (sigBits > ipBits)
		throw InvalidNrSigBits();

	if (sigBits < ipBits)
	{
		unsigned int partBytePos = (sigBits / 8U);
		unsigned int sigBitsInPartByte = (sigBits % 8U);
		unsigned int orMask = (1U << (8U - sigBitsInPartByte)) - 1U;
		ip[partBytePos] |= orMask;
		for (unsigned int k=partBytePos+1; k!=ipBytes; ++k)
			ip[k] = 0xFF;
	}
}


void IpToCountry::IncrementIp(unsigned char* ip, unsigned int ipBytes)
{
	for (int k=(int)ipBytes-1; k>=0; --k)
		if (++ip[k] != 0)
			break;
}


void IpToCountry::CallOnce(unsigned int volatile& callOnceState, std::function<void()> callee)
{
	if (InterlockedExchangeAdd(&callOnceState, 0) < CallOnceThreshold)
	{
		if (InterlockedIncrement(&callOnceState) == 1)
		{
			try
			{
				callee();
			}
			catch (...)
			{
				InterlockedExchange(&callOnceState, CallOnceThreshold);
				throw;
			}

			InterlockedExchange(&callOnceState, CallOnceThreshold);
		}
		else
		{
			while (InterlockedExchangeAdd(&callOnceState, 0) < CallOnceThreshold)
				SwitchToThread();
		}
	}
}
