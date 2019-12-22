#pragma once

#include "AtAuto.h"
#include "AtImfMsgWriter.h"
#include "AtMimeGrammar.h"
#include "AtNameValuePairs.h"
#include "AtOpt.h"
#include "AtToken.h"


namespace At
{
	namespace Mime
	{
		using Imf::MsgWriter;
		using Imf::MsgSectionScope;


		struct FieldWithParams
		{
			InsensitiveNameValuePairs m_params;

			void ReadParam(ParseNode const& fieldNode, PinStore& store);
			void WriteParams(MsgWriter& writer) const;
		};


		struct ContentType : public FieldWithParams
		{
			Seq m_type;
			Seq m_subType;

			ContentType() = default;
			ContentType(Seq type, Seq subType) noexcept : m_type(type), m_subType(subType) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;

			bool EqualType(Seq t)            const { return m_type.EqualInsensitive(t); }
			bool EqualSubType(Seq t, Seq st) const { return m_type.EqualInsensitive(t) && m_subType.EqualInsensitive(st); }

			bool IsTextPlain            () const { return EqualSubType ("text",        "plain"                 ); }
			bool IsTextHtml             () const { return EqualSubType ("text",        "html"                  ); }
			bool IsMultipart            () const { return EqualType    ("multipart"                            ); }
			bool IsMultipartMixed       () const { return EqualSubType ("multipart",   "mixed"                 ); }
			bool IsMultipartReport      () const { return EqualSubType ("multipart",   "report"                ); }
			bool IsMultipartAlternative () const { return EqualSubType ("multipart",   "alternative"           ); }
			bool IsMultipartRelated     () const { return EqualSubType ("multipart",   "related"               ); }
			bool IsMultipartFormData    () const { return EqualSubType ("multipart",   "form-data"             ); }
			bool IsAppWwwFormUrlEncoded () const { return EqualSubType ("application", "x-www-form-urlencoded" ); }
			bool IsMsgDeliveryStatus    () const { return EqualSubType ("message",     "delivery-status"       ); }
		};


		struct ContentEnc
		{
			Seq m_value;

			ContentEnc() = default;
			ContentEnc(Seq value) noexcept : m_value(value) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct ContentId
		{
			Seq m_id;

			ContentId() = default;
			ContentId(Seq id) noexcept : m_id(id) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};

		
		struct ContentDesc
		{
			Seq m_desc;

			ContentDesc() = default;
			ContentDesc(Seq desc) noexcept : m_desc(desc) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct ContentDisp : public FieldWithParams
		{
			Seq m_type;

			ContentDisp() = default;
			ContentDisp(Seq type) noexcept : m_type(type) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct ExtensionField
		{
			Seq m_name;
			Seq m_value;

			ExtensionField() = default;
			ExtensionField(Seq name, Seq value) noexcept : m_name(name), m_value(value) {}

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct MimeVersion
		{
			uint16 m_major = { 1 };
			uint16 m_minor = {};

			void Read(ParseNode const& fieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct PartReadErr
		{
			Vec<sizet> m_errPartPath;
			Str m_errDesc;

			void EncObj(Enc& enc) const;
		};


		// HTML emails in multipart format use up to 3 levels of nesting depth:
		// * depth=1: multipart/mixed; contains multipart/alternative as first part, followed by non-multipart attachments
		// * depth=2: multipart/alternative; contains text/plain as first part, followed by HTML within multipart/related
		// * depth=3: multipart/related; contains text/html as first part, followed by non-multipart inline content referenced by HTML
		// In addition, some implementations may encapsulate this in a fourth layer or more, adding a legal disclaimer of some sort.

		enum { MaxAutoDecodeDepth = 5 };	// Decodes up to 5 multipart levels automatically by default (1, 2, 3, 4 and 5)

		struct PartReadCx
		{
			bool             m_stopOnNestedErr {};
			bool             m_verboseParseErrs {};
			sizet            m_decodeDepth { MaxAutoDecodeDepth };
			Vec<sizet>       m_curPartPath;
			Vec<PartReadErr> m_errs;

			bool AddParseErr(ParseTree const& pt)
				{ PartReadErr& x = m_errs.Add(); x.m_errPartPath = m_curPartPath; x.m_errDesc.Obj(pt, ParseTree::BestAttempt); return false; }

			bool AddDecodeDepthErr()
				{ PartReadErr& x = m_errs.Add(); x.m_errPartPath = m_curPartPath; x.m_errDesc.Set("Multipart decode depth exceeded"); return false; }

			// Appends error descriptions as one or more lines terminated by "\r\n". Appends nothing if m_errs is empty
			void EncObj(Enc& enc) const;
		};


		struct Part;

		struct MultipartBody
		{
			Vec<Part> m_parts;

			bool Read(Seq& encoded, PinStore& store, PartReadCx& prcx);
			void Write(MsgWriter& writer, Seq boundary) const;
		};


		struct Part
		{
			Seq m_srcText;

			mutable Opt<ContentType> m_contentType;		// If IsMultipart(), WriteMimeFields adds "boundary" parameter if not present

			Opt<ContentEnc>     m_contentEnc;
			Opt<ContentId>      m_contentId;
			Opt<ContentDesc>    m_contentDesc;
			Opt<ContentDisp>    m_contentDisp;
			Opt<MimeVersion>    m_mimeVersion;
			Vec<ExtensionField> m_extensionFields;

			Seq m_contentEncoded;
			MultipartBody m_multipartBody;

			bool IsMultipart() const { return m_contentType.Any() && m_contentType->IsMultipart(); }

			// Called from Read/WritePart[Header], and also from Imf::Message::Read/Write
			bool TryReadMimeField(ParseNode const& fieldNode, PinStore& store);
			void WriteMimeFields(MsgWriter& writer) const;

			// Called from ReadPart
			void ReadPartHeader(ParseNode const& partHeaderNode, PinStore& store);			

			// Called from MultipartBody::Read, Write
			bool ReadPart(ParseNode const& partNode, PinStore& store, PartReadCx& prcx);
			void WritePart(MsgWriter& writer) const;

			// Avoid calling (will assert) for parts of type IsMultipart(). Those must use identity encoding ("7bit", "8bit" or "binary").
			// "storage" is written to only if a non-identity encoding is used. Returns false if Content-Transfer-Encoding not recognized.
			bool DecodeContent(Seq& decoded, PinStore& store) const;

		private:
			mutable Str m_boundaryStorage;

			Seq DecodeContent_QP     (PinStore& store) const;
			Seq DecodeContent_Base64 (PinStore& store) const;
		};


		struct PartContent
		{
			bool m_success;
			Seq  m_decoded;

			PartContent(Part const& part, PinStore& store) { m_success = part.DecodeContent(m_decoded, store); }
		};

	}
}
