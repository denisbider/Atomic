// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


void OgnSeqVec_FromStringVec(std::vector<std::string> const& strs, std::vector<OgnSeq>& seqs, size_t& nrSeqs, OgnSeq const*& seqPtr)
{
	seqs.clear();
	seqs.reserve(strs.size());

	for (std::string const& s : strs)
		seqs.emplace_back(OgnSeq_FromString(s));

	nrSeqs = seqs.size();
	if (nrSeqs)
		seqPtr = &seqs[0];
	else
		seqPtr = nullptr;
}


int HexDigitValue(char c)
{
	if (c >= '0' && c <= '9') return      (c - '0');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	throw StrErr(__FUNCTION__ ": Unexpected hex digit");
}


std::string String_FromHex(std::string const& x)
{
	std::string s;
	s.reserve(x.size() / 2);

	for (size_t i=0; i!=x.size(); ++i)
	{
		int hi = HexDigitValue(x[i]);
		if (++i == x.size()) throw StrErr(__FUNCTION__ ": Incomplete hex character");
		int lo = HexDigitValue(x[i]);
		s.append(1, (char) ((hi << 4) | lo));
	}

	return s;
}


std::string String_ToHex(std::string const& x)
{
	char const* const c_alphabet = "0123456789abcdef";
	std::string s;
	s.reserve(2 * x.size());

	for (char c : x)
	{
		s.append(1, c_alphabet[(((unsigned int) c) >> 4)  & 0x0F]);
		s.append(1, c_alphabet[(((unsigned int) c)     )  & 0x0F]);
	}

	return s;
}


bool StringToBool(std::string const& s, bool& v)
{
	if (s == "y") { v = true;  return true; }
	if (s == "n") { v = false; return true; }
	return false;
}


bool StringToUInt32Dec(std::string const& s, uint32_t& v)
{
	if (s.empty())
		return false;

	char* endPtr;
	v = strtoul(s.c_str(), &endPtr, 10);
	return !*endPtr;
}


bool StringToUInt64Dec(std::string const& s, uint64_t& v)
{
	if (s.empty())
		return false;

	char* endPtr;
	v = strtoull(s.c_str(), &endPtr, 10);
	return !*endPtr;
}


bool StringToVecOfUInt64Dec(std::string const& s, std::vector<uint64_t>& values)
{
	char const* startPtr = s.c_str();
	char* endPtr;

	while (*startPtr)
	{
		uint64_t v = strtoull(startPtr, &endPtr, 10);
		if (endPtr == startPtr)
			return false;

		values.emplace_back(v);
		if (!*endPtr)
			return true;

		while (*endPtr == ' ' || *endPtr == '\t' ) ++endPtr;
		if    (*endPtr == ','                    ) ++endPtr;
		while (*endPtr == ' ' || *endPtr == '\t' ) ++endPtr;
	}

	return true;
}


void OgnCheck(OgnResult const& r)
{
	FreeStorage freeStorage { r.m_storage };

	if (!r.m_success)
		if (!std::uncaught_exception())
			throw OgnErr(String_FromOgn(r.m_errDesc));
}


std::string LoadWholeFile(std::string const& inFile)
{
	std::ifstream ifs { inFile.c_str(), std::ios::in | std::ios::binary };
	if (!ifs)
		throw StrErr(__FUNCTION__ ": Could not open input file: " + inFile);

	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string result = ss.str();
	ifs.close();

	return result;
}


std::string LoadDkimPrivKeyFromFile(std::string const& kpFile)
{
	std::string encoded = LoadWholeFile(kpFile);
	size_t endOfLinePos = encoded.find("\r\n", 0);
	std::string privKeyHex = encoded.substr(0, endOfLinePos);
	return String_FromHex(privKeyHex);
}
