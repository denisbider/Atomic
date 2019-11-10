// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


std::mutex g_mxOutput;
using Locker = std::lock_guard<std::mutex>;


std::string GenTimeStamp()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	std::string nowStr;
	nowStr.resize(13);
	int nowStrLen = sprintf_s(&nowStr[0], nowStr.size(), "%02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	if (nowStrLen < 0)
		nowStr = "[Time error]";	// 12 chars
	else
		nowStr.resize((size_t) nowStrLen);

	return nowStr;
}


void __cdecl OgnTest_LogEvent(void*, uint32_t eventType, OgnSeq text)
{
	std::string now = GenTimeStamp();

	char const* zEventType;
		 if (eventType == EVENTLOG_ERROR_TYPE       ) zEventType = "[ERR] ";
	else if (eventType == EVENTLOG_WARNING_TYPE     ) zEventType = "[Warn]";
	else if (eventType == EVENTLOG_INFORMATION_TYPE ) zEventType = "[Info]";
	else                                              zEventType = "[?]   ";

	Locker locker { g_mxOutput };
	std::cout << now << " " << zEventType << " " << String_FromOgn(text) << std::endl;
}


void DescribeMsg(OgnMsgToSend const& msg)
{
	std::cout << "From: " << String_FromOgn(msg.m_fromAddress) << ", To: ";

	for (size_t i=0; i!=msg.m_nrPendingMailboxes; ++i)
	{
		if (i != 0)
			std::cout << ", ";

		std::cout << String_FromOgn(msg.m_pendingMailboxes[i]) << "@" << String_FromOgn(msg.m_toDomain);
	}

	std::cout << ", Size: " << msg.m_content.n;
}


void OgnTest_ShowMessages(size_t nrMsgs, OgnMsgToSend const* msgs)
{
	for (size_t i=0; i!=nrMsgs; ++i)
	{
		DescribeMsg(msgs[i]);
		std::cout << std::endl;
	}
}


void __cdecl OgnTest_OnReset(void*, size_t nrMsgs, OgnMsgToSend const* msgs)
{
	std::string now = GenTimeStamp();
	
	Locker locker { g_mxOutput };
	std::cout << now << " Reset: " << nrMsgs << " messages" << std::endl;
	OgnTest_ShowMessages(nrMsgs, msgs);
}


void __cdecl OgnTest_OnAttempt(void*, OgnMsgToSend const& msg)
{
	std::string now = GenTimeStamp();
	
	Locker locker { g_mxOutput };
	std::cout << now << " Attempt: ";
	DescribeMsg(msg);
	std::cout << std::endl;
}


void __cdecl OgnTest_OnResult(void*, OgnMsgToSend const& msg, size_t nrMailboxResults, OgnMbxResult const* mailboxResults, OgnTlsAssurance::E tlsAssuranceAchieved)
{
	std::string now = GenTimeStamp();
	
	Locker locker { g_mxOutput };
	std::cout << now << " Result: ";
	DescribeMsg(msg);
	std::cout << ", Status: ";

	for (size_t i=0; i!=nrMailboxResults; ++i)
	{
		OgnMbxResult const& mr = mailboxResults[i];
		if (i != 0)
			std::cout << ", ";

		std::cout << String_FromOgn(mr.m_mailbox) << " - " << OgnDeliveryState_Name(mr.m_state);
		if (mr.m_state == OgnDeliveryState::Success)
		{
			if (mr.m_successMx.n)
				std::cout << " (MX: " << String_FromOgn(mr.m_successMx) << ")";
		}
		else
		{
			if (mr.m_failure)
			{
				OgnSendFailure const& f = *mr.m_failure;
				std::cout << " (" << OgnSendStage_Name(f.m_stage) << ", " << OgnSendErr_Name(f.m_err);
				if (f.m_mx.n)		std::cout << ", MX: "		<< String_FromOgn(f.m_mx);
				if (f.m_replyCode)	std::cout << ", Reply: "	<< f.m_replyCode;
				if (f.m_enhStatus)	std::cout << ", Enh: "		<< f.m_enhStatus;
				if (f.m_desc.n)		std::cout << ", Desc: "		<< String_FromOgn(f.m_desc);
				if (f.m_nrLines)	std::cout << ", Line 1: "	<< String_FromOgn(f.m_lines[0]);
				std::cout << ")";
			}
		}
	}

	std::cout << ", TLS: " << OgnTlsAssurance_Name(tlsAssuranceAchieved) << std::endl;
}


bool __cdecl OgnTest_EnumMsgs(void*, size_t nrMsgs, OgnMsgToSend const* msgs)
{
	std::string now = GenTimeStamp();
	
	Locker locker { g_mxOutput };
	std::cout << now << " Enum: " << nrMsgs << " messages" << std::endl;
	OgnTest_ShowMessages(nrMsgs, msgs);
	return true;
}


bool __cdecl OgnTest_ClearMsgs(void*, size_t nrMsgs, OgnMsgToSend const* msgs)
{
	size_t nrCleared {}, nrIssues {};
	for (size_t i=0; i!=nrMsgs; ++i)
		if (msgs[i].m_status == OgnMsgStatus::NonFinal_Idle)
		{
			OgnRemoveResult::E removeResult;
			OgnResult or = Originator_RemoveIdleMessage(msgs[i].m_entityId, removeResult);
			if (or.m_success && removeResult == OgnRemoveResult::Found_Removed)
				++nrCleared;
			else
				++nrIssues;
		}

	std::string now = GenTimeStamp();
	
	Locker locker { g_mxOutput };
	std::cout << now << " Clear: " << nrCleared << " idle messages cleared, " << nrIssues << " issues" << std::endl;
	return true;
}


void CmdRun_Inner(std::string const& stgsFile, std::function<void()> funcToRunOnStartup)
{
	// Load settings file
	std::ifstream ifsStgs;

	char const* zStgsFile;
	if (stgsFile.empty())
		zStgsFile = "OgnTestSmtp.txt";
	else
		zStgsFile = stgsFile.c_str();
		
	ifsStgs.open(zStgsFile, std::ios::in | std::ios::binary);
	if (!ifsStgs)
		throw UsageErr("Could not open SMTP settings file: " + std::string(zStgsFile));

	std::string stgsContent;

	{
		std::stringstream ss;
		ss << ifsStgs.rdbuf();
		stgsContent = ss.str();
		ifsStgs.close();
	}

	// Parse settings file
	std::string senderComputerName, ipVerPrefStr, useRelayStr;
	std::string relayHost, relayPortStr, relayImplicitTlsStr, relayTlsReqStr, relayAuthTypeStr, relayUsername, relayPassword;

	StrView reader { stgsContent };
	while (true)
	{
		reader.SkipWs();
		if (!reader.Any())
			break;

		StrView line = reader.ReadToNewLine();
		if (line.StartsWithI("#"))
			continue;		// Comment

		line.RightTrimWs();
		StrView name = line.ReadToWs();
		line.SkipWs();

		     if (name.EqualI("senderComputerName" )) senderComputerName  = line.Str();
		else if (name.EqualI("ipVerPref"          )) ipVerPrefStr        = line.Str();
		else if (name.EqualI("useRelay"           )) useRelayStr         = line.Str();
		else if (name.EqualI("relayHost"          )) relayHost           = line.Str();
		else if (name.EqualI("relayPort"          )) relayPortStr        = line.Str();
		else if (name.EqualI("relayImplicitTls"   )) relayImplicitTlsStr = line.Str();
		else if (name.EqualI("relayTlsReq"        )) relayTlsReqStr      = line.Str();
		else if (name.EqualI("relayAuthType"      )) relayAuthTypeStr    = line.Str();
		else if (name.EqualI("relayUsername"      )) relayUsername       = line.Str();
		else if (name.EqualI("relayPassword"      )) relayPassword       = line.Str();
		else
			throw UsageErr("Unrecognized SMTP settings file directive: " + name.Str());
	}

	// Set up SMTP settings
	OgnSmtpSettings smtpStgs;
	smtpStgs.m_senderComputerName = OgnSeq_FromString(senderComputerName);
	smtpStgs.m_relayHost          = OgnSeq_FromString(relayHost);
	smtpStgs.m_relayUsername      = OgnSeq_FromString(relayUsername);

	// Convert parameters that aren't strings
	if (!OgnIpVerPref_NameToVal(OgnSeq_FromString(ipVerPrefStr), smtpStgs.m_ipVerPref))
		throw UsageErr("Unrecognized ipVerPref setting: " + ipVerPrefStr);

	if (!useRelayStr.empty())
		if (!StringToBool(useRelayStr, smtpStgs.m_useRelay))
			throw UsageErr("Unrecognized useRelay setting: " + useRelayStr);

	if (!relayPortStr.empty())
		if (!StringToUInt32Dec(relayPortStr, smtpStgs.m_relayPort) || smtpStgs.m_relayPort < 1 || smtpStgs.m_relayPort > 65535)
			throw UsageErr("Unrecognized relayPort setting: " + relayPortStr);

	if (!relayImplicitTlsStr.empty())
		if (!StringToBool(relayImplicitTlsStr, smtpStgs.m_relayImplicitTls))
			throw UsageErr("Unrecognized relayImplicitTls setting: " + relayImplicitTlsStr);

	if (!relayTlsReqStr.empty())
		if (!OgnTlsAssurance_NameToVal(OgnSeq_FromString(relayTlsReqStr), smtpStgs.m_relayTlsRequirement))
			throw UsageErr("Unrecognized relayTlsReq setting: " + relayTlsReqStr);

	if (!relayAuthTypeStr.empty())
		if (!OgnAuthType_NameToVal(OgnSeq_FromString(relayAuthTypeStr), smtpStgs.m_relayAuthType))
			throw UsageErr("Unrecognized relayAuthType setting: " + relayAuthTypeStr);

	ProtectedPassword pw { relayPassword };
	smtpStgs.m_relayPassword = pw.ToOgnSeq();

	// Clear memory that may contain a password. We can't make this completely effective because we used
	// iostreams to read file content and intermediate buffers aren't accessible for us to clear
	SecureClearString(relayPassword);
	SecureClearString(stgsContent);

	// Set SMTP settings
	OgnCheck(Originator_SetSmtpSettings(smtpStgs));

	// Determine store directory
	std::string storeDir;
	storeDir.resize(MAX_PATH);
	DWORD moduleFileNameLen = GetModuleFileNameA(NULL, &storeDir[0], (DWORD) storeDir.size());
	if (ERROR_SUCCESS != GetLastError())
		throw StrErr(__FUNCTION__ ": Error in GetModuleFileName");
	
	storeDir.resize(moduleFileNameLen);
	while (storeDir.size() && storeDir.back() != '\\')
		storeDir.pop_back();

	storeDir.append("OgnStore");

	// Set up service settings
	OgnServiceSettings serviceStgs;
	serviceStgs.m_storeDir  = OgnSeq_FromString(storeDir);
	serviceStgs.m_logEvent  = OgnTest_LogEvent;
	serviceStgs.m_onReset   = OgnTest_OnReset;
	serviceStgs.m_onAttempt = OgnTest_OnAttempt;
	serviceStgs.m_onResult  = OgnTest_OnResult;

	OgnCheck(Originator_SetServiceSettings(serviceStgs));

	// Start the Originator service
	OgnCryptInitializer ognCryptInit;

	OgnCheck(Originator_Start());

	{ Locker locker { g_mxOutput }; std::cout << "Originator started" << std::endl; }

	funcToRunOnStartup();

	{ Locker locker { g_mxOutput }; std::cout << "Press Esc or Q to exit; Space to show messages; C to clear unsent" << std::endl; }

	while (true)
	{
		int c = _getch();
		if (c == 27 || c == 'q' || c == 'Q')
			break;

		if (c == ' ')
			OgnCheck(Originator_EnumMessages(OgnTest_EnumMsgs, nullptr));
		else if (c == 'c' || c == 'C')
			OgnCheck(Originator_EnumMessages(OgnTest_ClearMsgs, nullptr));
	}

	{ Locker locker { g_mxOutput }; std::cout << "Stopping Originator" << std::endl; }

	OgnCheck(Originator_BeginStop());

	bool stopped {};
	OgnCheck(Originator_WaitStop(INFINITE, stopped));

	{
		Locker locker { g_mxOutput };
		if (stopped)
			std::cout << "Originator stopped." << std::endl;
		else
			std::cout << "Originator did NOT stop." << std::endl;
	}
}


void CmdRun(CmdlArgs& args)
{
	std::string stgsFile;

	while (args.More())
	{
		std::string arg = args.Next();

		if (arg == "-stgs")
			stgsFile = args.Next();
		else
			throw UsageErr("Unrecognized parameter: " + arg);
	}

	CmdRun_Inner(stgsFile, [] {} );
}
