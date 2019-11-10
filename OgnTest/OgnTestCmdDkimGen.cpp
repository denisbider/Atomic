// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


void CmdDkimGen(std::string const& outFile)
{
	std::ofstream ofs { outFile.c_str(), std::ios::out | std::ios::trunc | std::ios::binary };
	if (!ofs)
		throw StrErr(__FUNCTION__ ": Could not open output file: " + outFile);

	OgnCryptInitializer cryptInit;
	OgnDkimKeypair kp;
	OgnCheck(Originator_GenerateDkimKeypair(kp));
	FreeStorage freeStorage { kp.m_storage };

	ofs << String_ToHex(String_FromOgn(kp.m_privKeyBin)).c_str() << "\r\n"
		<< String_FromOgn(kp.m_pubKeyText).c_str() << "\r\n";
}
