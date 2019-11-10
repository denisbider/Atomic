// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#pragma once

#include "OgnTestIncludes.h"


struct StrErr : std::exception
{
	StrErr(std::string const& s) : m_s(s) {}
	char const* what() const { return m_s.c_str(); }
	std::string m_s;
};


struct UsageErr : StrErr { UsageErr(std::string const& s) : StrErr(s) {} };
struct OgnErr : StrErr { OgnErr(std::string const& s) : StrErr(s) {} };


struct NoCopy
{
	NoCopy() = default;

private:
	NoCopy(NoCopy const&) = delete;
	void operator= (NoCopy const&) = delete;
};


struct FreeStorage : NoCopy
{
	FreeStorage(void* storage) : m_storage(storage) {}
	~FreeStorage() { if (m_storage) CoTaskMemFree(m_storage); }

private:
	void* m_storage {};
};


template <typename T>
struct FreeObjVecStorage : NoCopy
{
	FreeObjVecStorage(std::vector<T>& objs) : m_objs(objs) {}

	~FreeObjVecStorage()
	{
		for (T& x : m_objs)
			if (x.m_storage)
			{
				CoTaskMemFree(x.m_storage);
				x.m_storage = nullptr;
			}
	}

private:
	std::vector<T>& m_objs;
};



inline OgnSeq OgnSeq_FromString(std::string const& x) { if (x.empty()) return OgnSeq(); else return OgnSeq_Make(&x[0], x.size()); }
void OgnSeqVec_FromStringVec(std::vector<std::string> const& strs, std::vector<OgnSeq>& seqs, size_t& nrSeqs, OgnSeq const*& seqPtr);
inline std::string String_FromOgn(OgnSeq x) { return std::string { x.p, x.n }; }

int HexDigitValue(char c);
std::string String_FromHex(std::string const& x);
std::string String_ToHex(std::string const& x);

bool StringToBool(std::string const& s, bool& v);
bool StringToUInt32Dec(std::string const& s, uint32_t& v);
bool StringToUInt64Dec(std::string const& s, uint64_t& v);
bool StringToVecOfUInt64Dec(std::string const& s, std::vector<uint64_t>& values);

void OgnCheck(OgnResult const& r);

std::string LoadWholeFile(std::string const& inFile);
std::string LoadDkimPrivKeyFromFile(std::string const& kpFile);



struct OgnCryptInitializer : NoCopy
{
	OgnCryptInitializer() { OgnCheck(Originator_CryptInit()); }
	~OgnCryptInitializer() { OgnCheck(Originator_CryptFree()); }
};



class CmdlArgs
{
public:
	CmdlArgs(int argc, char const* const* argv) : m_argc(argc), m_argv(argv) {}

	bool More() const { return m_cur < m_argc; }

	void Skip()
	{
		if (m_cur < m_argc)
			++m_cur;
	}

	std::string Next()
	{
		if (m_cur == m_argc)
			throw UsageErr("Unexpected end of command line arguments");

		return m_argv[m_cur++];
	}

private:
	int                m_argc {};
	char const* const* m_argv {};
	int                m_cur  {};
};



class StrView
{
public:
	StrView(std::string const& s) : m_s(s), m_len(s.size()) {}
	StrView(StrView const& x) : m_s(x.m_s), m_ofs(x.m_ofs), m_len(x.m_len) {}

	bool Any() const { return m_len != 0; }

	std::string Str() const { return m_s.substr(m_ofs, m_len); }

	void SkipWs() { SkipAnyOf(" \t\r\n"); }

	void SkipAnyOf(char const* z)
	{
		while (m_len)
		{
			int c = m_s[m_ofs];
			if (!strchr(z, c))
				break;

			++m_ofs;
			--m_len;
		}
	}

	StrView ReadToWs      () { return ReadToFirstOf(" \t\r\n" ); }
	StrView ReadToNewLine () { return ReadToFirstOf("\r\n"    ); }

	StrView ReadToFirstOf(char const* z)
	{
		size_t newLineOfs = m_s.find_first_of(z, m_ofs);
		size_t nrToRead;
		if (newLineOfs == std::string::npos || newLineOfs >= m_ofs + m_len)
			nrToRead = m_len;
		else
			nrToRead = newLineOfs - m_ofs;

		StrView result { m_s, m_ofs, nrToRead };
		m_ofs += nrToRead;
		m_len -= nrToRead;
		return result;
	}

	void RightTrimWs() { return RightTrimAnyOf(" \t\r\n"); }

	void RightTrimAnyOf(char const* z)
	{
		while (m_len)
		{
			int c = m_s[m_ofs + m_len - 1];
			if (!strchr(z, c))
				break;

			--m_len;
		}
	}

	bool EqualI(char const* z)
	{
		size_t zLen = strlen(z);
		if (m_len != zLen) return false;
		return !_memicmp(&m_s[m_ofs], z, zLen);
	}

	bool StartsWithI(char const* z)
	{
		size_t zLen = strlen(z);
		if (m_len < zLen) return false;
		return !_memicmp(&m_s[m_ofs], z, zLen);
	}

private:
	StrView(std::string const& s, size_t ofs, size_t len) : m_s(s), m_ofs(ofs), m_len(len) {}

	std::string const& m_s;
	size_t             m_ofs {};
	size_t             m_len;
};



class ProtectedPassword
{
public:
	ProtectedPassword(std::string const& pw)
	{
		DATA_BLOB in { (DWORD) pw.size(), (BYTE*) &pw[0] };
		if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &m_out))
			throw StrErr(__FUNCTION__ ": Error in CryptProtectData");
	}

	~ProtectedPassword() noexcept(false)
	{
		if (m_out.pbData)
		{
			if (LocalFree(m_out.pbData) != nullptr)
				if (!std::uncaught_exception())
					throw StrErr(__FUNCTION__ ": Error in LocalFree");

			m_out.cbData = 0;
			m_out.pbData = nullptr;
		}
	}

	OgnSeq ToOgnSeq() { return OgnSeq_Make((char const*) m_out.pbData, m_out.cbData); }

private:
	DATA_BLOB m_out {};
};



inline void SecureClearString(std::string& x)
{
	SecureZeroMemory(&x[0], x.size());
	x.clear();
}



void CmdEnums();
void CmdAddrs(std::string const& inFile);
void CmdDkimGen(std::string const& outFile);
void CmdDkimPub(std::string const& kpFile);
void CmdGenMsg(CmdlArgs& args);
void CmdRun_Inner(std::string const& stgsFile, std::function<void()> funcToRunOnStartup);
void CmdRun(CmdlArgs& args);
void CmdSendMsg(CmdlArgs& args);
