// Copyright (C) 2018-2019 by denis bider. All rights reserved.

#include "OgnTestIncludes.h"
#include "OgnTestCommon.h"


char const* const c_mkdnDefault =
	"# Test\r\n"
	"\r\n"
	"## Code\r\n"
	"\r\n"
	"    foo\r\n"
	"    bar baz\r\n"
	"\r\n"
	"The quick `brown` fox *jumped* over the lazy **dog**.\r\n"
	"\r\n"
	"  ### Ordered list\r\n"
	"\r\n"
	"1. First item\r\n"
	"\r\n"
	"2. Second item\r\n"
	"\r\n"
	"3. Third item\r\n"
	"\r\n"
	"   ### Not a heading but still third item\r\n"
	"\r\n"
	"  ### Unordered list\r\n"
	"\r\n"
	"- First item\r\n"
	"- Second item\r\n"
	"- Third item\r\n"
	"\r\n"
	"##### Not a heading but a paragraph\r\n"
	"\r\n"
	"    ### Also not a heading but code\r\n"
	"\r\n"
	"#### Text\r\n"
	"\r\n"
	"At least one paragraph in the example should be somewhat longer so that word wrap behavior can be observed. "
	"It would be suitable if such a paragraph ran multiple lines and had some *italic* and **bold** text also. "
	"The paragraph could also contain some `code keywords`. Now links:\r\n"
	"\r\n"
	"https://www.example.com/1\r\n"
	"\r\n"
	" https://www.example.com/2\r\n"
	"\r\n"
	"  https://www.example.com/3\r\n"
	"\r\n"
	"   https://www.example.com/4\r\n"
	"\r\n"
	"    https://www.example.com/not_a_link_but_instead_code\r\n"
	"\r\n"
	"Final paragraph.\r\n";


void CmdGenMsg(CmdlArgs& args)
{
	std::string outFile, mkdnFile, fromAddr, subject, kpFile, sdid, selector;
	std::vector<std::string> toAddrs, ccAddrs, attachTypes, attachFiles;

	while (args.More())
	{
		std::string arg = args.Next();

			 if (arg == "-o"      ) outFile  = args.Next();
		else if (arg == "-mkdn"   ) mkdnFile = args.Next();
		else if (arg == "-from"   ) fromAddr = args.Next();
		else if (arg == "-to"     ) toAddrs.emplace_back(args.Next());
		else if (arg == "-cc"     ) ccAddrs.emplace_back(args.Next());
		else if (arg == "-sub"    ) subject  = args.Next();
		else if (arg == "-kp"     ) kpFile   = args.Next();
		else if (arg == "-sdid"   ) sdid     = args.Next();
		else if (arg == "-sel"    ) selector = args.Next();
		else if (arg == "-attach" ) { attachTypes.emplace_back(args.Next()); attachFiles.emplace_back(args.Next()); }
		else
			throw UsageErr("Unrecognized parameter: " + arg);
	}

	if (fromAddr.empty()) fromAddr = "from@example.com";

	// Open output file, if any
	std::ofstream ofs;

	if (!outFile.empty())
	{
		ofs.open(outFile.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
		if (!ofs)
			throw UsageErr("Could not open output file: " + outFile);
	}

	std::ostream& out = ofs.is_open() ? ofs : std::cout;

	// Load markdown input file, if any
	std::ifstream ifsMkdn;

	if (!mkdnFile.empty())
	{
		ifsMkdn.open(mkdnFile.c_str(), std::ios::in | std::ios::binary);
		if (!ifsMkdn)
			throw UsageErr("Could not open markdown input file: " + mkdnFile);
	}

	std::string mkdn;
	if (!ifsMkdn.is_open())
		mkdn = c_mkdnDefault;
	else
	{
		std::stringstream ss;
		ss << ifsMkdn.rdbuf();
		mkdn = ss.str();
		ifsMkdn.close();
	}

	// Initialize From: address if not provided
	if (fromAddr.empty())
		fromAddr = "example@example.com";

	// Load DKIM keypair, if any
	std::string privKeyBin;
	if (!kpFile.empty())
		privKeyBin = LoadDkimPrivKeyFromFile(kpFile);

	// Set up message headers
	OgnMsgHeaders hdrs;

	// From address
	OgnCheck(Originator_SplitMailbox(OgnSeq_FromString(fromAddr), hdrs.m_fromMailbox));
	FreeStorage freeFromMailbox { hdrs.m_fromMailbox.m_storage };

	// To addresses
	std::vector<OgnMailbox> toMailboxes;
	FreeObjVecStorage<OgnMailbox> freeToMailboxes { toMailboxes };
	toMailboxes.reserve(toAddrs.size());

	for (std::string const& addr : toAddrs)
	{
		toMailboxes.emplace_back();
		OgnCheck(Originator_SplitMailbox(OgnSeq_FromString(addr), toMailboxes.back()));
	}

	hdrs.m_nrToMailboxes = toMailboxes.size();
	if (toMailboxes.size())
		hdrs.m_toMailboxes = &toMailboxes[0];

	// CC addresses
	std::vector<OgnMailbox> ccMailboxes;
	FreeObjVecStorage<OgnMailbox> freeCcMailboxes { ccMailboxes };
	ccMailboxes.reserve(ccAddrs.size());

	for (std::string const& addr : ccAddrs)
	{
		ccMailboxes.emplace_back();
		OgnCheck(Originator_SplitMailbox(OgnSeq_FromString(addr), ccMailboxes.back()));
	}

	hdrs.m_nrCcMailboxes = ccMailboxes.size();
	if (ccMailboxes.size())
		hdrs.m_ccMailboxes = &ccMailboxes[0];
	
	// Simple string fields
	hdrs.m_subject        = OgnSeq_FromString(subject);
	hdrs.m_dkimSdid       = OgnSeq_FromString(sdid);
	hdrs.m_dkimSelector   = OgnSeq_FromString(selector);
	hdrs.m_dkimPrivKeyBin = OgnSeq_FromString(privKeyBin);

	// Attachments
	std::vector<std::string> attachBodies;
	for (std::string const& attachFile : attachFiles)
	{
		std::ifstream ifsAttachFile;
		ifsAttachFile.open(attachFile.c_str(), std::ios::in | std::ios::binary);
		if (!ifsAttachFile)
			throw UsageErr("Could not open attachment file: " + attachFile);

		std::stringstream ss;
		ss << ifsAttachFile.rdbuf();
		attachBodies.emplace_back(ss.str());
		ifsAttachFile.close();
	}

	std::vector<OgnMsgPart> attachParts;
	attachParts.reserve(attachTypes.size());

	for (size_t i=0; i!=attachTypes.size(); ++i)
	{
		attachParts.emplace_back();
		attachParts.back().m_contentType = OgnSeq_FromString(attachTypes[i]);
		attachParts.back().m_content = OgnSeq_FromString(attachBodies[i]);
	}

	OgnMsgParts attachments;
	attachments.m_nrParts = attachParts.size();
	if (attachParts.size())
		attachments.m_parts = &attachParts[0];

	// Generate message
	OgnStringResult content;
	OgnCryptInitializer cryptInit;
	OgnCheck(Originator_GenerateMessageFromMarkdown(hdrs, OgnSeq_FromString(mkdn), attachments, content));
	FreeStorage freeStorage { content.m_storage };

	// Output result
	out << String_FromOgn(content.m_str);
}
