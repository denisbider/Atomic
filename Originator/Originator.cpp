#include "OgnIncludes.h"

#include "OgnConversions.h"
#include "OgnGlobals.h"



// DLL interface functions

void ParseAndValidate(ParseFunc parseFunc, OgnSeq ognSrcText, OgnValidResult& result)
{
	Seq srcText = Seq_FromOgn(ognSrcText);
	ParseTree pt { srcText } ;
	if (pt.Parse(parseFunc))
		result.m_valid = true;
	else
	{
		result.m_valid        = false;
		result.m_bestToRow    = pt.BestToRow();
		result.m_bestToColumn = pt.BestToCol();

		EnsureThrow(pt.BestRemaining().p >= srcText.p);
		result.m_bestToOffset = (size_t) (pt.BestRemaining().p - srcText.p);
		EnsureThrow(result.m_bestToOffset <= srcText.n);
	}
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_IsValidDomain(OgnSeq domain, OgnValidResult& result)
{
	ORIGINATOR_FUNC_BEGIN
		ParseAndValidate(Imf::C_domain, domain, result);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_IsValidCasualEmailAddressList(OgnSeq casualAddressList, OgnValidResult& result)
{
	ORIGINATOR_FUNC_BEGIN
		ParseAndValidate(Imf::C_casual_addr_list, casualAddressList, result);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_ForEachAddressInCasualEmailAddressList(OgnSeq casualAddressList, OgnStringResult& parseErr, OgnFn_EnumStr fn, void* fnCx)
{
	ORIGINATOR_FUNC_BEGIN
		Imf::ForEachAddressResult r = Imf::ForEachAddressInCasualEmailAddressList(Seq_FromOgn(casualAddressList), [fn, fnCx] (Str&& addr) -> bool
			{ return fn(fnCx, OgnSeq_FromSeq(addr)); } );

		if (r.m_parseError.Any())
			OgnStringResult_FromSeq(parseErr, r.m_parseError);
		else
			parseErr = OgnStringResult();
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_IsValidEmailAddress(OgnSeq address, OgnValidResult& result)
{
	ORIGINATOR_FUNC_BEGIN
		ParseAndValidate(Imf::C_addr_spec, address, result);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_IsValidMailbox(OgnSeq mailbox, OgnValidResult& result)
{
	ORIGINATOR_FUNC_BEGIN
		ParseAndValidate(Imf::C_mailbox, mailbox, result);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_SplitEmailAddress(OgnSeq address, OgnEmailAddress& parts)
{
	ORIGINATOR_FUNC_BEGIN
		Str localPart, domain;
		if (!Imf::ExtractLocalPartAndDomainFromEmailAddress(Seq_FromOgn(address), &localPart, &domain))
		{
			parts.m_storage   = nullptr;
			parts.m_localPart = OgnSeq();
			parts.m_domain    = OgnSeq();
		}
		else
		{
			enum { NrMappings = 2 };
			StorageMapping mappings[NrMappings] {
				{ parts.m_localPart, localPart },
				{ parts.m_domain,    domain    } };

			CopyResultToStorage(parts.m_storage, NrMappings, mappings);
		}
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_SplitMailbox(OgnSeq mailbox, OgnMailbox& parts)
{
	ORIGINATOR_FUNC_BEGIN
		ParseTree pt { Seq_FromOgn(mailbox) };
		if (!pt.Parse(Imf::C_mailbox))
		{
			parts.m_storage   = nullptr;
			parts.m_name      = OgnSeq();
			parts.m_localPart = OgnSeq();
			parts.m_domain    = OgnSeq();
		}
		else
		{
			PinStore pinStore { mailbox.n };

			Imf::Mailbox mbox;
			mbox.Read(pt.Root().FlatFindRef(Imf::id_mailbox), pinStore);

			Seq name;
			Seq localPart = mbox.m_addr.m_localPart;
			Seq domain = mbox.m_addr.m_domain;

			if (mbox.m_name.Any())
				name = mbox.m_name->m_value;

			enum { NrMappings = 3 };
			StorageMapping mappings[NrMappings] = {
				{ parts.m_name,      name      },
				{ parts.m_localPart, localPart },
				{ parts.m_domain,    domain    } };

			CopyResultToStorage(parts.m_storage, NrMappings, mappings);
		}
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_CryptInit()
{
	ORIGINATOR_FUNC_BEGIN
		Crypt::Init();
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_CryptFree()
{
	ORIGINATOR_FUNC_BEGIN
		Crypt::Free();
	ORIGINATOR_FUNC_CLOSE
}


void ExportDkimPubKey(RsaSigner& signer, Str& pubText)
{
	Seq pubPrefix { "h=sha256; p=" };
	Str pubBin;

	signer.ExportPubPkcs8(pubBin);

	Base64::NewLines newLines { 96, pubPrefix.n };
	pubText.Clear().ReserveExact(pubPrefix.n + newLines.MaxOutLen(Base64::EncodeBaseLen(pubBin.Len())));
	pubText.Add(pubPrefix);
	Base64::MimeEncode(pubBin, pubText, Base64::Padding::Yes, newLines);
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_GenerateDkimKeypair(OgnDkimKeypair& keypair)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(Crypt::Inited());
		Str privBin;

		RsaSigner signer;
		signer.Generate(2048);
		signer.ExportPriv(privBin);

		Str pubText;
		ExportDkimPubKey(signer, pubText);

		enum { NrMappings = 2 };
		StorageMapping mappings[NrMappings] = {
			{ keypair.m_privKeyBin, privBin },
			{ keypair.m_pubKeyText, pubText } };

		CopyResultToStorage(keypair.m_storage, NrMappings, mappings);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_GetDkimPubKeyFromPrivKey(OgnSeq privKeyBin, OgnStringResult& pubKeyText)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(Crypt::Inited());

		RsaSigner signer;
		signer.ImportPriv(Seq_FromOgn(privKeyBin));

		Str pubText;
		ExportDkimPubKey(signer, pubText);

		OgnStringResult_FromSeq(pubKeyText, pubText);
	ORIGINATOR_FUNC_CLOSE
}


void Originator_GenerateMessageFromParts_Inner(OgnMsgHeaders const& headers, OgnMsgPart const& body, OgnStringResult& content)
{
	PinStore pinStore { 1000 };
	Imf::Message msg;

	// Date
	msg.m_date.Init(Time::NonStrictNow());

	// From: address
	ImfAddressList_FromOgn(msg.m_from.Init().m_addrList, 1, &headers.m_fromMailbox);

	// To: addresses
	if (headers.m_nrToMailboxes)
		ImfAddressList_FromOgn(msg.m_to.Init().m_addrList, headers.m_nrToMailboxes, headers.m_toMailboxes);

	// Cc: addresses
	if (headers.m_nrCcMailboxes)
		ImfAddressList_FromOgn(msg.m_cc.Init().m_addrList, headers.m_nrCcMailboxes, headers.m_ccMailboxes);

	// Subject
	msg.m_subject.Init(Seq_FromOgn(headers.m_subject));

	// Auto-Submitted
	if (OgnAutoSbmtd::Invalid != headers.m_autoSbmtd)
		msg.m_autoSbmtd.Init((Imf::AutoSbmtdValue) headers.m_autoSbmtd);

	// Message-ID
	Str msgId;
	Imf::MsgId::Generate(msgId, Seq_FromOgn(headers.m_fromMailbox.m_domain));
	msg.m_msgId.Init(msgId);

	// Body
	OgnPartStats stats;
	MimePart_FromOgn(msg, stats, body, pinStore);

	// Encode the message
	Str msgContent;
	msgContent.ReserveExact(1000 + (200 * headers.m_nrToMailboxes) + (500 * stats.m_nrParts) + stats.m_totalContentBytes);
	Imf::MsgWriter msgWriter { msgContent };
	msg.Write(msgWriter);

	if (!headers.m_dkimPrivKeyBin.n)
		OgnStringResult_FromSeq(content, msgContent);
	else
	{
		// DKIM sign the message
		RsaSigner signer;
		signer.ImportPriv(Seq_FromOgn(headers.m_dkimPrivKeyBin));

		Str signedMsg;
		signedMsg.ReserveExact(1000 + msgContent.Len());
		Dkim::SignMessage(signer, Seq_FromOgn(headers.m_dkimSdid), Seq_FromOgn(headers.m_dkimSelector), msgContent, signedMsg);
		signedMsg.Add(msgContent);

		OgnStringResult_FromSeq(content, signedMsg);
	}
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_GenerateMessageFromParts(OgnMsgHeaders const& headers, OgnMsgPart const& body, OgnStringResult& content)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(Crypt::Inited());
		Originator_GenerateMessageFromParts_Inner(headers, body, content);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_GenerateMessageFromMarkdown(OgnMsgHeaders const& headers, OgnSeq markdown, OgnMsgParts const& attachments, OgnStringResult& content)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(Crypt::Inited());

		Seq mkdn = Seq_FromOgn(markdown);
		Markdown::Transform transform { mkdn };
		if (!transform.Parse())
		{
			Markdown::Transform transform2 { mkdn };
			ParseTree& tree = transform2.Tree();
			tree.RecordBestToStack();
			EnsureThrow(!transform2.Parse());
			throw StrErr(Str("Could not parse markdown: ").Obj(tree, ParseTree::BestAttempt));
		}

		TextBuilder text { 2 * mkdn.n };
		transform.ToText(text);
		Seq textStr = text.Final();

		HtmlBuilder html { 500 + 2 * mkdn.n };
		transform.SetHtmlPermitLinks(true);
		transform.ToHtml(html);
		Seq htmlStr = html.EndHtml().Final();

		// The plain text and HTML representation are bundled into a single multipart group with MIME type "multipart/alternative"
		enum { NrBundleParts = 2 };
		OgnMsgPart bundleParts[NrBundleParts];
		bundleParts[0].m_contentType = OgnSeq_FromZ("text/plain; charset=UTF-8");
		bundleParts[0].m_content     = OgnSeq_FromSeq(textStr);
		bundleParts[1].m_contentType = OgnSeq_FromZ("text/html; charset=UTF-8");
		bundleParts[1].m_content     = OgnSeq_FromSeq(htmlStr);

		auto initBundle = [&] (OgnMsgPart& part)
			{
				part.m_contentType = OgnSeq_FromZ("multipart/alternative");
				part.m_isNested = true;
				part.m_nested.m_nrParts = NrBundleParts;
				part.m_nested.m_parts = bundleParts;
			};

		// If there are no attachments, the "multipart/alternative" bundle becomes the parent part.
		// If there are attachments, the "multipart/alternative" bundle becomes the first child of "multipart/mixed" with attachments following.
		Vec<OgnMsgPart> bodyParts;
		OgnMsgPart body;

		if (!attachments.m_nrParts)
			initBundle(body);
		else
		{
			bodyParts.ReserveExact(1 + attachments.m_nrParts);

			initBundle(bodyParts.Add());
			for (sizet i=0; i!=attachments.m_nrParts; ++i)
				bodyParts.Add(attachments.m_parts[i]);

			body.m_contentType = OgnSeq_FromZ("multipart/mixed");
			body.m_isNested = true;
			body.m_nested.m_nrParts = bodyParts.Len();
			body.m_nested.m_parts = bodyParts.Ptr();
		}

		Originator_GenerateMessageFromParts_Inner(headers, body, content);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_GetServiceState(OgnServiceState::E& state)
{
	ORIGINATOR_FUNC_BEGIN
		state = (OgnServiceState::E) g_ognServiceState;
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_SetServiceSettings(OgnServiceSettings const& settings)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(g_ognServiceState == OgnServiceState::NotStarted);

		// Workaround for spurious VS 2015 "unreachable code" warning
		OgnStoredServiceSettings* pStored = new OgnStoredServiceSettings;
		Rp<OgnStoredServiceSettings> stored { pStored };

		stored->m_storeDir.Set(settings.m_storeDir.p, settings.m_storeDir.n);
		stored->m_openOversizeFilesTarget = settings.m_openOversizeFilesTarget;
		stored->m_cachedPagesTarget       = settings.m_cachedPagesTarget;
		stored->m_logEvent                = settings.m_logEvent;
		stored->m_onReset                 = settings.m_onReset;
		stored->m_onAttempt               = settings.m_onAttempt;
		stored->m_onResult                = settings.m_onResult;
		g_ognServiceSettings.Set(stored);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_SetSmtpSettings(OgnSmtpSettings const& settings)
{
	ORIGINATOR_FUNC_BEGIN
		Rp<OgnSmtpCfg> cfg = new OgnSmtpCfg;
		cfg->m_senderComputerName                  =           Seq_FromOgn(settings.m_senderComputerName);
		cfg->m_smtpSenderCfg.f_ipVerPreference     =  (IpVerPreference::E) settings.m_ipVerPref;
		cfg->m_smtpSenderCfg.f_useRelay            =                       settings.m_useRelay;
		cfg->m_smtpSenderCfg.f_relayHost           =           Seq_FromOgn(settings.m_relayHost);
		cfg->m_smtpSenderCfg.f_relayPort           =                       settings.m_relayPort;
		cfg->m_smtpSenderCfg.f_relayImplicitTls    =                       settings.m_relayImplicitTls;
		cfg->m_smtpSenderCfg.f_relayTlsRequirement = (SmtpTlsAssurance::E) settings.m_relayTlsRequirement;
		cfg->m_smtpSenderCfg.f_relayAuthType       =     (MailAuthType::E) settings.m_relayAuthType;
		cfg->m_smtpSenderCfg.f_relayUsername       =           Seq_FromOgn(settings.m_relayUsername);
		cfg->m_smtpSenderCfg.f_relayPassword       =           Seq_FromOgn(settings.m_relayPassword);
		g_ognSmtpCfg.Set(cfg);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_Start()
{
	ORIGINATOR_FUNC_BEGIN
		long prevState = g_ognServiceState;
		EnsureThrow(OgnServiceState::NotStarted == prevState || OgnServiceState::Stopped == prevState);
		EnsureThrow(prevState == _InterlockedCompareExchange(&g_ognServiceState, OgnServiceState::Starting, prevState));

		EnsureThrow(Crypt::Inited());

		Rp<OgnStoredServiceSettings> settings = g_ognServiceSettings.Get();
		EnsureThrow(settings.Any());
		EnsureThrow(g_ognSmtpCfg.Get().Any());

		{
			// Workaround for spurious VS 2015 "unreachable code" warning
			StopCtl* pStopCtl = new StopCtl();
			g_ognStopCtl.Set(pStopCtl);
		}

		g_ognStore.Set(new EntityStore);
		g_ognStore->SetDirectory               (settings->m_storeDir);
		g_ognStore->SetOpenOversizeFilesTarget (settings->m_openOversizeFilesTarget);
		g_ognStore->SetCachedPagesTarget       (settings->m_cachedPagesTarget);
		g_ognStore->Init();

		g_ognStore->RunTxExclusive( [] {
			g_ognSmtpSenderStorage = g_ognStore->InitCategoryParent<OgnSmtpSenderStorage>(); } );

		g_ognSmtpSender.Create();
		g_ognSmtpSender->SetStopCtl(g_ognStopCtl);
		g_ognSmtpSender->Start();

		EnsureThrow(OgnServiceState::Starting == _InterlockedCompareExchange(&g_ognServiceState, OgnServiceState::Started, OgnServiceState::Starting));
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_SendMessage(OgnMsgToSend const& msg)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(OgnServiceState::Started == g_ognServiceState);
		EnsureThrow(0 == msg.m_entityId.m_uniqueId);
		EnsureThrow(0 == msg.m_entityId.m_index);
		EnsureThrow(OgnMsgStatus::NonFinal_Idle == msg.m_status);

		Rp<SmtpMsgToSend> smtpMsg = g_ognSmtpSender->CreateMsg();

		smtpMsg->f_nextAttemptTime    =          Time::FromFt(msg.m_nextAttemptTime);
		smtpMsg->f_tlsRequirement     = (SmtpTlsAssurance::E) msg.m_tlsRequirement;
		smtpMsg->f_fromAddress        =           Seq_FromOgn(msg.m_fromAddress);
		smtpMsg->f_toDomain           =           Seq_FromOgn(msg.m_toDomain);
		smtpMsg->f_contentPart1       =           Seq_FromOgn(msg.m_content);
		smtpMsg->f_deliveryContext    =           Seq_FromOgn(msg.m_deliveryContext);

		if (msg.m_customTimeout)
		{
			smtpMsg->f_baseSendSecondsMax = msg.m_baseSendSecondsMax;
			smtpMsg->f_nrBytesToAddOneSec = msg.m_nrBytesToAddOneSec;
		}

		if (msg.m_customRetrySchedule)
		{
			smtpMsg->f_futureRetryDelayMinutes.Clear();
			for (sizet i=0; i!=msg.m_nrFutureRetryDelayMinutes; ++i)
				smtpMsg->f_futureRetryDelayMinutes.Add(msg.m_futureRetryDelayMinutes[i]);
		}
		
		for (sizet i=0; i!=msg.m_nrAdditionalMatchDomains; ++i) smtpMsg->f_additionalMatchDomains .Add(Seq_FromOgn(msg.m_additionalMatchDomains [i]));
		for (sizet i=0; i!=msg.m_nrPendingMailboxes;       ++i) smtpMsg->f_pendingMailboxes       .Add(Seq_FromOgn(msg.m_pendingMailboxes       [i]));

		g_ognStore->RunTx(g_ognStopCtl, typeid(OgnTx_SendMessage), [&] {
			g_ognSmtpSender->Send(smtpMsg); } );
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_EnumMessages(OgnFn_EnumMsgs fn, void* fnCx)
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(OgnServiceState::Started == g_ognServiceState);

		struct EnumState
		{
			RpVec<SmtpMsgToSend> m_msgs;
		};

		auto onMatch = [] (EnumState& state, Rp<SmtpMsgToSend> const& msg)
			{ state.m_msgs.Add(msg); };

		auto finalizeBatch = [] (EnumState&) {};

		auto afterCommit = [fn, fnCx] (EnumState& state) -> bool
			{
				Vec<OgnMsgToSend> ognMsgs;
				Vec<OgnMsgStorage> storage;

				ognMsgs.ReserveExact(state.m_msgs.Len());
				storage.ReserveExact(state.m_msgs.Len());

				for (Rp<SmtpMsgToSend> const& msg : state.m_msgs)
					OgnMsgToSend_FromSmtp(ognMsgs.Add(), storage.Add(), msg.Ref());
				
				return fn(fnCx, ognMsgs.Len(), ognMsgs.Ptr());
			};

		g_ognStore->MultiTx_ProcessAllChildrenOfKind<SmtpMsgToSend, EnumState>(Exclusive::No, g_ognStopCtl, g_ognSmtpSenderStorage->m_entityId,
			onMatch, finalizeBatch, afterCommit);
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_RemoveIdleMessage(OgnObjId entityId, OgnRemoveResult::E& result)
{
	ORIGINATOR_FUNC_BEGIN
		result = OgnRemoveResult::None;

		OgnRemoveResult::E txResult = OgnRemoveResult::None;

		g_ognStore->RunTx(g_ognStopCtl, typeid(OgnTx_RemoveIdleMessage), [entityId, &txResult]
			{
				txResult = OgnRemoveResult::None;

				Rp<SmtpMsgToSend> msg = g_ognStore->GetEntityOfKind<SmtpMsgToSend>(ObjId_FromOgn(entityId), ObjId::None);
				if (!msg.Any())
					txResult = OgnRemoveResult::NotFound;
				else if (msg->f_status != SmtpMsgStatus::NonFinal_Idle)
					txResult = OgnRemoveResult::Found_CannotRemove;
				else
				{
					msg->Remove();
					txResult = OgnRemoveResult::Found_Removed;
				}
			} );

		result = txResult;
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_BeginStop()
{
	ORIGINATOR_FUNC_BEGIN
		EnsureThrow(OgnServiceState::Started == _InterlockedCompareExchange(&g_ognServiceState, OgnServiceState::Stop_Waiting, OgnServiceState::Started));
		g_ognStopCtl->Stop(__FUNCTION__ ": Stop requested");
	ORIGINATOR_FUNC_CLOSE
}


ORIGINATOR_FUNC OgnResult __cdecl Originator_WaitStop(uint32_t waitMs, bool& stopped)
{
	ORIGINATOR_FUNC_BEGIN
		if (OgnServiceState::Stop_Waiting == g_ognServiceState)
			stopped = g_ognStopCtl->WaitAll(waitMs);
		else
		{
			EnsureThrow(OgnServiceState::Stop_Deinitializing == g_ognServiceState || OgnServiceState::Stopped == g_ognServiceState);
			stopped = true;
		}

		if (stopped)
		{
			if (OgnServiceState::Stop_Waiting == _InterlockedCompareExchange(&g_ognServiceState, OgnServiceState::Stop_Deinitializing, OgnServiceState::Stop_Waiting))
			{
				// This thread must deinitialize
				g_ognStore.Set(nullptr);
				g_ognServiceState = OgnServiceState::Stopped;
			}
			else
			{
				while (OgnServiceState::Stopped != g_ognServiceState)
				{
					// Another thread is deinitializing. Wait for it to complete
					Sleep(0);
				}
			}
		}
	ORIGINATOR_FUNC_CLOSE
}
