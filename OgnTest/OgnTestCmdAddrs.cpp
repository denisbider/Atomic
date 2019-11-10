// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


bool __cdecl CmdAddrs_StoreAddr(void* pvAddresses, OgnSeq addr)
{
	std::vector<std::string>& addresses = *((std::vector<std::string>*) pvAddresses);
	addresses.emplace_back(String_FromOgn(addr));
	return true;
}


void CmdAddrs(std::string const& inFile)
{
	if (inFile.empty())
	{
		std::cout	<< "\"Name M. Surname\" <aa@bb>; group:aa@bb,cc@dd;, First" << std::endl
					<< " Last <xx@yy>,; \";,_\" (uff) @ (f;u,f) example.com,,, \";,_\" <\";,_\" (uff) @ (f;u,f) example.com>" << std::endl
					<< "aa@bb" << std::endl
					<< "  Name Surname <cc@dd>  " << std::endl
					<< "multiline" << std::endl
					<< " @example.com; Foo Bar <multiline(" << std::endl
					<< " )@example.com>; \"Foo Bar\" <multiline@(" << std::endl
					<< " )example.com> <zz@zz> \"[First Last]\" <ww@ww>zz@ee<foo@bar>Last<last@example.com>" << std::endl;
	}
	else
	{
		std::string addressList = LoadWholeFile(inFile);
		std::vector<std::string> addresses;
		OgnStringResult parseErr;
		OgnCheck(Originator_ForEachAddressInCasualEmailAddressList(OgnSeq_FromString(addressList), parseErr, CmdAddrs_StoreAddr, &addresses));

		if (parseErr.m_storage)
		{
			FreeStorage freeStorage { parseErr.m_storage };
			std::cout << String_FromOgn(parseErr.m_str) << std::endl;
		}
		else
		{
			std::cout << addresses.size() << " addresses" << std::endl;
			if (!addresses.empty())
			{
				for (size_t i=0; i!=addresses.size(); ++i)
					std::cout << i << ": " << addresses[i] << std::endl;

				std::cout	<< std::endl
							<< "Address parts:" << std::endl;

				for (size_t i=0; i!=addresses.size(); ++i)
				{
					OgnMailbox parts;
					OgnCheck(Originator_SplitMailbox(OgnSeq_FromString(addresses[i]), parts));
					FreeStorage freeStorage { parts.m_storage };
					std::cout	<< i << ": name=<" << String_FromOgn(parts.m_name)
								<< ">, localPart=<" << String_FromOgn(parts.m_localPart)
								<< ">, domain=<" << String_FromOgn(parts.m_domain) << ">" << std::endl;
				}
			}
		}
	}
}
