#pragma once

#include "OgnIncludes.h"



// DLL interface function prologue and epilogue

#define ORIGINATOR_FUNC_BEGIN try {
#define ORIGINATOR_FUNC_CLOSE return OgnResult_Success(); } catch (std::exception const& e) { return OgnResult_FromStdException(__FUNCTION__, e); }



// OgnSeq

inline Seq Seq_FromOgn(OgnSeq x)
{
	return Seq(x.p, x.n);
}


inline OgnSeq OgnSeq_FromSeq(Seq x)
{
	return OgnSeq_Make((char const*) x.p, x.n);
}


inline OgnSeq OgnSeq_FromZ(char const* z)
{
	return OgnSeq_Make(z, ZLen(z));
}



// OgnObjId

inline OgnObjId OgnObjId_FromObjId(ObjId x)
{
	OgnObjId o;
	o.m_uniqueId = x.m_uniqueId;
	o.m_index    = x.m_index;
	return o;
}


inline ObjId ObjId_FromOgn(OgnObjId x)
{
	ObjId o;
	o.m_uniqueId = x.m_uniqueId;
	o.m_index    = x.m_index;
	return o;
}



// CopyResultToStorage

struct StorageMapping
{
	OgnSeq&		m_dest;
	Seq			m_src;

	StorageMapping(OgnSeq& dest, Seq src) : m_dest(dest), m_src(src) {}
};

// Storage must be freed using CoTaskMemFree, presumably by the using application.
// If any Originator code runs after this that may throw exceptions or return errors, care must be taken to free storage
void CopyResultToStorage(void*& storage, int nrMappings, StorageMapping const* mappings);



// OgnSeqVec

void OgnSeqVec_FromStrVec(Vec<OgnSeq>& ognVec, Vec<Str> const& strVec);



// OgnResult

inline OgnResult OgnResult_Success()
{
	return OgnResult { true, nullptr };
}


OgnResult OgnResult_FromSeq_Verbatim(Seq s);
OgnResult OgnResult_FromStdException(char const* funcName, std::exception const& e);



// OgnStringResult

// Storage must be freed using CoTaskMemFree, presumably by the using application.
// If any Originator code runs after this that may throw exceptions or return errors, care must be taken to free storage
void OgnStringResult_FromSeq(OgnStringResult& r, Seq x);



// OgnMailbox

void ImfMailbox_FromOgn(Imf::Mailbox& mbox, OgnMailbox const& ognMailbox);
void ImfAddressList_FromOgn(Imf::AddressList& addrList, sizet nrMailboxes, OgnMailbox const* mailboxes);



// OgnMsgPart

struct OgnPartStats
{
	sizet m_nrParts {};
	sizet m_totalContentBytes {};
};

void MimePart_FromOgn(Mime::Part& part, OgnPartStats& stats, OgnMsgPart const& ognPart, PinStore& pinStore);



// OgnSendFailure

struct OgnSendFailureStorage
{
	Vec<OgnSeq> m_lines;
};

void OgnSendFailure_FromSmtp(OgnSendFailure& ognFailure, OgnSendFailureStorage& storage, SmtpSendFailure const& smtpFailure);



// OgnMbxResult

struct OgnMbxResultStorage
{
	OgnSendFailure			m_failure;
	OgnSendFailureStorage	m_failureStorage;
};

void OgnMbxResult_FromSmtp(OgnMbxResult& ognMbxResult, OgnMbxResultStorage& storage, MailboxResult const& mailboxResult);
void OgnMbxResults_FromSmtp(Vec<OgnMbxResult>& ognMbxResults, Vec<OgnMbxResultStorage>& storage, Vec<MailboxResult> const& mailboxResults);



// OgnMsgToSend

struct OgnMsgStorage
{
	Vec<OgnSeq>              m_additionalMatchDomains;
	Vec<OgnSeq>              m_pendingMailboxes;
	Vec<OgnMbxResult>        m_mailboxResults;
	Vec<OgnMbxResultStorage> m_mailboxResultsStorage;
};

void OgnMsgToSend_FromSmtp(OgnMsgToSend& ognMsg, OgnMsgStorage& storage, SmtpMsgToSend const& smtpMsg);
