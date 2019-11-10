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


		struct Part;

		struct MultipartBody
		{
			Vec<Part> m_parts;

			bool Read(Seq& encoded, PinStore& store, sizet depth = 0);
			void Read(ParseTree const& ptBody, PinStore& store, sizet depth = 0);
			void Write(MsgWriter& writer, Seq boundary) const;
		};


		struct Part
		{
			// HTML emails in multipart format use up to 3 levels of nesting depth:
			// * depth=0: multipart/mixed; contains multipart/alternative as first part, followed by non-multipart attachments
			// * depth=1: multipart/alternative; contains text/plain as first part, followed by HTML within multipart/related
			// * depth=2: multipart/related; contains text/html as first part, followed by non-multipart inline content referenced by HTML

			enum { MaxAutoDecodeDepth = 3 };	// Decodes up to 4 multipart levels automatically by default (0, 1, 2, and 3)

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
			void ReadPart(ParseNode const& partNode, PinStore& store, sizet depth = 0);
			void WritePart(MsgWriter& writer) const;

			// Avoid calling (will assert) for parts of type IsMultipart(). Those must use identity encoding ("7bit", "8bit" or "binary").
			// "storage" is written to only if a non-identity encoding is used. Returns false if Content-Transfer-Encoding not recognized.
			bool DecodeContent(Seq& decoded, Str& storage) const;

		private:
			mutable Str m_boundaryStorage;

			void DecodeContent_QP     (Enc& enc) const;
			void DecodeContent_Base64 (Enc& enc) const;
		};


		struct PartContent
		{
			bool m_success;
			Seq  m_decoded;
			Str  m_storage;

			PartContent(Part const& part) { m_success = part.DecodeContent(m_decoded, m_storage); }
		};

	}
}
