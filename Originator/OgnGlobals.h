#pragma once

#include "OgnSmtpSender.h"


struct OgnTx_SendMessage {};
struct OgnTx_EnumMessages {};
struct OgnTx_RemoveIdleMessage {};


struct OgnStoredServiceSettings : RefCountable
{
	Str				m_storeDir;
	size_t			m_openOversizeFilesTarget {};
	size_t			m_cachedPagesTarget       {};

	OgnFn_LogEvent	m_logEvent                {};
	OgnFn_OnReset	m_onReset                 {};
	OgnFn_OnAttempt	m_onAttempt               {};
	OgnFn_OnResult	m_onResult                {};
	void*           m_callbackCx              {};		// Context passed back as first parameter when above functions are invoked
};


struct OgnSmtpCfg : RefCountable
{
	Str				m_senderComputerName;
	SmtpSenderCfg	m_smtpSenderCfg { Entity::Contained };
};


extern long volatile						g_ognServiceState;
extern RpAnchor<OgnStoredServiceSettings>	g_ognServiceSettings;

extern Rp<StopCtl>							g_ognStopCtl;
extern AutoFree<EntityStore>				g_ognStore;
extern ThreadPtr<OgnSmtpSender>				g_ognSmtpSender;

extern RpAnchor<OgnSmtpCfg>					g_ognSmtpCfg;

extern Rp<OgnSmtpSenderStorage>				g_ognSmtpSenderStorage;
