#include "OgnIncludes.h"
#include "OgnGlobals.h"


long volatile						g_ognServiceState {};
RpAnchor<OgnStoredServiceSettings>	g_ognServiceSettings;

Rp<StopCtl>							g_ognStopCtl;
AutoFree<EntityStore>				g_ognStore;
ThreadPtr<OgnSmtpSender>			g_ognSmtpSender;

RpAnchor<OgnSmtpCfg>				g_ognSmtpCfg;

Rp<OgnSmtpSenderStorage>			g_ognSmtpSenderStorage;
