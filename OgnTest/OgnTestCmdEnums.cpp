// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


void PrintEnumValInfo(OgnEnumValInfo const& evi)
{
	std::cout << evi.m_value << ", " << evi.m_name << ", " << evi.m_desc << std::endl;
}


typedef OgnResult (__cdecl * OgnEnumGetValuesFunc)(OgnEnumValues&);

void GetAndPrintEnumValues(char const* name, OgnEnumGetValuesFunc getter)
{
	std::cout << std::endl
			  << name << std::endl;

	OgnEnumValues ev;
	FreeStorage freeStorage { ev.m_storage };

	OgnCheck(getter(ev));

	for (size_t i=0; i!=ev.m_nrValues; ++i)
		PrintEnumValInfo(ev.m_values[i]);
}


void CmdEnums()
{
	#define CMDENUMS_MAP(NAME) GetAndPrintEnumValues(#NAME, NAME##_Values)
	CMDENUMS_MAP(OgnIpVerPref);
	CMDENUMS_MAP(OgnTlsAssurance);
	CMDENUMS_MAP(OgnAuthType);
	CMDENUMS_MAP(OgnMsgStatus);
	CMDENUMS_MAP(OgnDeliveryState);
	CMDENUMS_MAP(OgnSendStage);
	CMDENUMS_MAP(OgnSendErr);
	#undef CMDENUMS_MAP
}
