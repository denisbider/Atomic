// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


void CmdSendMsg(CmdlArgs& args)
{
	bool haveRetryStr {};
	std::string stgsFile, retryStr, tlsReqStr, baseSecsMaxStr, minBpsStr, fromAddr, toDomain, contentFile, ctx;
	std::vector<std::string> addlDomainStrs, mboxStrs;

	while (args.More())
	{
		std::string arg = args.Next();

		     if (arg == "-stgs"        )   stgsFile       = args.Next();
		else if (arg == "-retry"       ) { retryStr       = args.Next(); haveRetryStr = true; }
		else if (arg == "-tlsreq"      )   tlsReqStr      = args.Next();
		else if (arg == "-basesecsmax" )   baseSecsMaxStr = args.Next();
		else if (arg == "-minbps"      )   minBpsStr      = args.Next();
		else if (arg == "-from"        )   fromAddr       = args.Next();
		else if (arg == "-todomain"    )   toDomain       = args.Next();
		else if (arg == "-content"     )   contentFile    = args.Next();
		else if (arg == "-ctx"         )   ctx            = args.Next();
		else if (arg == "-addldomain"  )   addlDomainStrs .emplace_back(args.Next());
		else if (arg == "-mbox"        )   mboxStrs       .emplace_back(args.Next());
		else
			throw UsageErr("Unrecognized parameter: " + arg);
	}

	// Set up OgnMsgToSend
	OgnMsgToSend msg;
	msg.m_fromAddress     = OgnSeq_FromString(fromAddr);
	msg.m_toDomain        = OgnSeq_FromString(toDomain);
	msg.m_deliveryContext = OgnSeq_FromString(ctx);

	std::vector<uint64_t> retryMins;
	if (haveRetryStr)
	{
		if (!StringToVecOfUInt64Dec(retryStr, retryMins))
			throw UsageErr("Unrecognized -retry parameter value: " + retryStr);

		msg.m_customRetrySchedule = true;
		msg.m_nrFutureRetryDelayMinutes = retryMins.size();
		if (msg.m_nrFutureRetryDelayMinutes)
			msg.m_futureRetryDelayMinutes = &retryMins[0];
	}

	if (!tlsReqStr.empty())
		if (!OgnTlsAssurance_NameToVal(OgnSeq_FromString(tlsReqStr), msg.m_tlsRequirement))
			throw UsageErr("Unrecognized -tlsreq parameter value: " + tlsReqStr);

	if (!baseSecsMaxStr.empty())
		if (!StringToUInt64Dec(baseSecsMaxStr, msg.m_baseSendSecondsMax))
			throw UsageErr("Unrecognized -basesecsmax parameter value: " + baseSecsMaxStr);

	if (!minBpsStr.empty())
		if (!StringToUInt64Dec(minBpsStr, msg.m_minSendBytesPerSec))
			throw UsageErr("Unrecognized -minbps parameter value: " + minBpsStr);

	std::vector<OgnSeq> addlDomainSeqs, mboxSeqs;
	OgnSeqVec_FromStringVec(addlDomainStrs, addlDomainSeqs, msg.m_nrAdditionalMatchDomains, msg.m_additionalMatchDomains );
	OgnSeqVec_FromStringVec(mboxStrs,       mboxSeqs,       msg.m_nrPendingMailboxes,       msg.m_pendingMailboxes       );

	// Load content from file
	std::string contentStr;

	if (!contentFile.empty())
	{
		std::ifstream ifsContent;
		ifsContent.open(contentFile.c_str(), std::ios::in | std::ios::binary);
		if (!ifsContent)
			throw UsageErr("Could not open message content file: " + contentFile);
	
		std::stringstream ss;
		ss << ifsContent.rdbuf();
		contentStr = ss.str();
		ifsContent.close();
	}

	msg.m_content = OgnSeq_FromString(contentStr);

	// Run Originator and send message
	CmdRun_Inner(stgsFile, [&]
		{
			OgnCheck(Originator_SendMessage(msg));
		} );
}
