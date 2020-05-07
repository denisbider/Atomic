#include "OgnIncludes.h"
#include "OgnConversions.h"



// CopyResultToStorage

void CopyResultToStorage(void*& storage, int nrMappings, StorageMapping const* mappings)
{
	sizet totalBytes {};
	for (int i=0; i!=nrMappings; ++i)
		totalBytes += mappings[i].m_src.n;

	if (!totalBytes)
		storage = nullptr;
	else
	{
		storage = CoTaskMemAlloc(totalBytes);
		if (!storage)
			throw std::bad_alloc();
	}

	char* pBegin = (char*) storage;
	char* pWrite = pBegin;
	for (int i=0; i!=nrMappings; ++i)
	{
		Seq src = mappings[i].m_src;
		if (!src.n)
			mappings[i].m_dest = OgnSeq();
		else
		{
			memcpy(pWrite, src.p, src.n);
			mappings[i].m_dest = OgnSeq_Make(pWrite, src.n);
			pWrite += src.n;
		}
	}

	EnsureAbort(pBegin + totalBytes == pWrite);
}



// OgnSeqVec

void OgnSeqVec_FromStrVec(Vec<OgnSeq>& ognVec, Vec<Str> const& strVec)
{
	ognVec.Clear().ReserveExact(strVec.Len());
	for (Str const& s : strVec)
		ognVec.Add(OgnSeq_FromSeq(s));
}



// OgnResult

OgnResult OgnResult_FromSeq_Verbatim(Seq s)
{
	OgnResult r { false };
	r.m_storage = CoTaskMemAlloc(s.n);
	if (!r.m_storage)
		r.m_errDesc = OgnSeq_FromZ(__FUNCTION__ ": CoTaskMemAlloc returned nullptr");
	else
	{
		// No exceptions beyond this point. Otherwise, if there could be an exception, must ensure to free storage

		memcpy(r.m_storage, s.p, s.n);
		r.m_errDesc.p = (char const*) r.m_storage;
		r.m_errDesc.n = s.n;
	}
	return r;
}


OgnResult OgnResult_FromStdException(char const* funcName, std::exception const& e)
{
	Str msg;
	msg.SetAdd(funcName, ": Exception: ", e.what());
	return OgnResult_FromSeq_Verbatim(msg);
}



// OgnStringResult

void OgnStringResult_FromSeq(OgnStringResult& r, Seq x)
{
	StorageMapping mapping { r.m_str, x };
	CopyResultToStorage(r.m_storage, 1, &mapping);
}



// OgnMailbox

void ImfMailbox_FromOgn(Imf::Mailbox& mbox, OgnMailbox const& ognMailbox)
{
	if (ognMailbox.m_name.n)
		mbox.m_name.Init(Seq_FromOgn(ognMailbox.m_name));
	else
		mbox.m_name.Clear();

	mbox.m_addr.m_localPart = Seq_FromOgn(ognMailbox.m_localPart);
	mbox.m_addr.m_domain    = Seq_FromOgn(ognMailbox.m_domain);
}


void ImfAddressList_FromOgn(Imf::AddressList& addrList, sizet nrMailboxes, OgnMailbox const* mailboxes)
{
	addrList.m_addresses.Clear();
	for (sizet i=0; i!=nrMailboxes; ++i)
	{
		Imf::Address& addr = addrList.m_addresses.Add();
		addr.m_type = Imf::Address::Type::Mailbox;
		ImfMailbox_FromOgn(addr.m_mbox, mailboxes[i]);
	}
}



// OgnMsgPart

void MimePart_FromOgn(Mime::Part& part, OgnPartStats& stats, OgnMsgPart const& ognPart, PinStore& pinStore)
{
	++stats.m_nrParts;

	part.m_mimeVersion.Init();

	Seq contentType = Seq_FromOgn(ognPart.m_contentType);
	ParseTree ptContentType { contentType };
	if (!ptContentType.Parse(Mime::C_content_type_inner))
		throw StrErr(Str(__FUNCTION__ ": Content type does not match grammar: ").Add(contentType));

	part.m_contentType.Init().Read(ptContentType.Root(), pinStore);

	if (!ognPart.m_isNested)
	{
		part.EncodeContent_Base64(Seq_FromOgn(ognPart.m_content), pinStore);
		stats.m_totalContentBytes += part.m_contentEncoded.n;
	}
	else
	{
		for (sizet i=0; i!=ognPart.m_nested.m_nrParts; ++i)
		{
			Mime::Part& nestedPart = part.AddChildPart();
			MimePart_FromOgn(nestedPart, stats, ognPart.m_nested.m_parts[i], pinStore);
		}
	}
}



// OgnSendFailure

void OgnSendFailure_FromSmtp(OgnSendFailure& ognFailure, OgnSendFailureStorage& storage, SmtpSendFailure const& smtpFailure)
{
	ognFailure.m_stage		= (OgnSendStage::E) smtpFailure.f_stage;
	ognFailure.m_err		=   (OgnSendErr::E) smtpFailure.f_err;
	ognFailure.m_mx			=    OgnSeq_FromSeq(smtpFailure.f_mx);
	ognFailure.m_replyCode	=                   smtpFailure.f_replyCode;
	ognFailure.m_enhStatus	=                   smtpFailure.f_enhStatus;
	ognFailure.m_desc		=    OgnSeq_FromSeq(smtpFailure.f_desc);

	OgnSeqVec_FromStrVec(storage.m_lines, smtpFailure.f_lines);
	ognFailure.m_nrLines = storage.m_lines.Len();
	ognFailure.m_lines = storage.m_lines.Ptr();
}



// OgnMbxResult

void OgnMbxResult_FromSmtp(OgnMbxResult& ognMbxResult, OgnMbxResultStorage& storage, MailboxResult const& mailboxResult)
{
	ognMbxResult.m_time      =                        mailboxResult.f_time.ToFt();
	ognMbxResult.m_mailbox   =         OgnSeq_FromSeq(mailboxResult.f_mailbox);
	ognMbxResult.m_state     = (OgnDeliveryState::E) (mailboxResult.f_state);
	ognMbxResult.m_successMx =         OgnSeq_FromSeq(mailboxResult.f_successMx);

	if (!mailboxResult.f_failure.Any())
		ognMbxResult.m_failure = nullptr;
	else
	{
		OgnSendFailure_FromSmtp(storage.m_failure, storage.m_failureStorage, mailboxResult.f_failure.Ref());
		ognMbxResult.m_failure = &storage.m_failure;
	}
}


void OgnMbxResults_FromSmtp(Vec<OgnMbxResult>& ognMbxResults, Vec<OgnMbxResultStorage>& storage, Vec<MailboxResult> const& mailboxResults)
{
	ognMbxResults.Clear().ReserveExact(mailboxResults.Len());
	storage.Clear().ReserveExact(mailboxResults.Len());

	for (MailboxResult const& result : mailboxResults)
		OgnMbxResult_FromSmtp(ognMbxResults.Add(), storage.Add(), result);
}



// OgnMsgToSend

void OgnMsgToSend_FromSmtp(OgnMsgToSend& ognMsg, OgnMsgStorage& storage, SmtpMsgToSend const& smtpMsg)
{
	ognMsg.m_entityId		 =   OgnObjId_FromObjId(smtpMsg.m_entityId);
	ognMsg.m_nextAttemptTime =                      smtpMsg.f_nextAttemptTime.ToFt();
	ognMsg.m_status			 =    (OgnMsgStatus::E) smtpMsg.f_status;
	ognMsg.m_tlsRequirement	 = (OgnTlsAssurance::E) smtpMsg.f_tlsRequirement;
	ognMsg.m_fromAddress	 =       OgnSeq_FromSeq(smtpMsg.f_fromAddress);
	ognMsg.m_toDomain		 =       OgnSeq_FromSeq(smtpMsg.f_toDomain);
	ognMsg.m_content		 =       OgnSeq_FromSeq(smtpMsg.f_contentPart1);
	ognMsg.m_deliveryContext =       OgnSeq_FromSeq(smtpMsg.f_deliveryContext);

	ognMsg.m_customTimeout      = true;
	ognMsg.m_baseSendSecondsMax	= smtpMsg.f_baseSendSecondsMax;
	ognMsg.m_nrBytesToAddOneSec	= smtpMsg.f_nrBytesToAddOneSec;

	ognMsg.m_customRetrySchedule       = true;
	ognMsg.m_nrFutureRetryDelayMinutes = smtpMsg.f_futureRetryDelayMinutes.Len();
	ognMsg.m_futureRetryDelayMinutes   = smtpMsg.f_futureRetryDelayMinutes.Ptr();

	OgnSeqVec_FromStrVec(storage.m_additionalMatchDomains, smtpMsg.f_additionalMatchDomains);
	ognMsg.m_nrAdditionalMatchDomains = storage.m_additionalMatchDomains.Len();
	ognMsg.m_additionalMatchDomains = storage.m_additionalMatchDomains.Ptr();

	OgnSeqVec_FromStrVec(storage.m_pendingMailboxes, smtpMsg.f_pendingMailboxes);
	ognMsg.m_nrPendingMailboxes = storage.m_pendingMailboxes.Len();
	ognMsg.m_pendingMailboxes = storage.m_pendingMailboxes.Ptr();

	OgnMbxResults_FromSmtp(storage.m_mailboxResults, storage.m_mailboxResultsStorage, smtpMsg.f_mailboxResults);
	ognMsg.m_nrMailboxResults = storage.m_mailboxResults.Len();
	ognMsg.m_mailboxResults = storage.m_mailboxResults.Ptr();
}
