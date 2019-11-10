// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


void CmdDkimPub(std::string const& kpFile)
{
	std::string privKeyBin = LoadDkimPrivKeyFromFile(kpFile);

	OgnCryptInitializer cryptInit;
	OgnStringResult pubKeyText;
	OgnCheck(Originator_GetDkimPubKeyFromPrivKey(OgnSeq_FromString(privKeyBin), pubKeyText));
	FreeStorage freeStorage { pubKeyText.m_storage };

	// Output result
	std::cout << String_FromOgn(pubKeyText.m_str) << std::endl;
}
