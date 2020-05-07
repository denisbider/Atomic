#pragma once

#include "AtImfDateTime.h"
#include "AtMimeReadWrite.h"
#include "AtOpt.h"


namespace At
{
	namespace Imf
	{

		// Strings

		Seq ReadQuotedString(ParseNode const& qsNode, PinStore& store);
		void WriteQuotedString(MsgWriter& writer, Seq value, Seq afterClose = Seq());


		Seq ReadPhrase(ParseNode const& phraseNode, PinStore& store);
		void WritePhrase(MsgWriter& writer, Seq value);

		struct Phrase
		{
			Seq m_value;

			Phrase() {}
			Phrase(Seq value) : m_value(value) {}

			void Read(ParseNode const& phraseNode, PinStore& store) { m_value = ReadPhrase(phraseNode, store); }
			void Write(MsgWriter& writer) const { WritePhrase(writer, m_value); }
		};


		Seq ReadDomain(ParseNode const& domainNode, PinStore& store);
		
		Seq ReadUnstructured(ParseNode const& unstrNode, PinStore& store);
		void WriteUnstructured(MsgWriter& writer, Seq value);



		// Addresses

		struct AddrSpec
		{
			Seq m_localPart;
			Seq m_domain;

			AddrSpec() {}
			AddrSpec(Seq localPart, Seq domain) : m_localPart(localPart), m_domain(domain) {}

			void Read(ParseNode const& addrSpecNode, PinStore& store);
			void Write(MsgWriter& writer) const;

			void EncObj(Enc& enc) const { enc.ReserveInc(m_localPart.n + 1 + m_domain.n).Add(m_localPart).Ch('@').Lower(m_domain); }
		};


		struct Mailbox
		{
			Opt<Phrase> m_name;
			AddrSpec m_addr;

			Mailbox() {}
			Mailbox(Seq localPart, Seq domain) : m_addr(localPart, domain) {}
			Mailbox(Seq name, Seq localPart, Seq domain) : m_name(name), m_addr(localPart, domain) {}

			void Read(ParseNode const& mailboxNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct Group
		{
			Seq m_name;
			Vec<Mailbox> m_mboxes;

			void Read(ParseNode const& groupNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct Address
		{
			struct Type { enum E { None, Mailbox, Group }; };

			Type::E m_type = { Type::None };
			Mailbox m_mbox;
			Group m_group;

			Address() {}
			Address(Type::E type) : m_type(type) {}
			Address(Mailbox const& mbox) : m_type(Type::Mailbox), m_mbox(mbox) {}

			void Read(ParseNode const& addressNode, PinStore& store);
			void Write(MsgWriter& writer) const;

			// Does NOT clear addrSpecs before adding to it
			sizet ExtractAddrSpecs(Vec<AddrSpec>& addrSpecs) const;
		};


		struct AddressList
		{
			Vec<Address> m_addresses;

			AddressList() {}

			template <typename First, typename... Rest>
			AddressList(First&& first, Rest&&... rest) { m_addresses.Add(Address(std::forward<First>(first), std::forward<Rest>(rest)...)); }

			void Read(ParseNode const& addressListNode, PinStore& store);
			void Write(MsgWriter& writer) const;

			// Does NOT clear addrSpecs before adding to it
			sizet ExtractAddrSpecs(Vec<AddrSpec>& addrSpecs) const;
		};



		// Message fields

		struct DateField
		{
			DateTime m_dt;

			DateField() {}
			DateField(Time t) : m_dt(t) {}

			void Read(ParseNode const& origDateNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct FromField
		{
			AddressList m_addrList;

			FromField() {}

			template <typename First, typename... Rest>
			FromField(First&& first, Rest&&... rest) : m_addrList(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& fromNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct SenderField
		{
			Address m_addr;

			SenderField() {}

			template <typename First, typename... Rest>
			SenderField(First&& first, Rest&&... rest) : m_addr(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& senderNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct ReplyToField
		{
			AddressList m_addrList;

			ReplyToField() {}

			template <typename First, typename... Rest>
			ReplyToField(First&& first, Rest&&... rest) : m_addrList(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& replyToNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct ToField
		{
			AddressList m_addrList;

			ToField() {}

			template <typename First, typename... Rest>
			ToField(First&& first, Rest&&... rest) : m_addrList(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& toNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct CcField
		{
			AddressList m_addrList;

			CcField() {}

			template <typename First, typename... Rest>
			CcField(First&& first, Rest&&... rest) : m_addrList(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& ccNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct BccField
		{
			AddressList m_addrList;

			BccField() {}

			template <typename First, typename... Rest>
			BccField(First&& first, Rest&&... rest) : m_addrList(std::forward<First>(first), std::forward<Rest>(rest)...) {} 

			void Read(ParseNode const& bccNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct MsgId
		{
			static void Generate(Enc& enc, Seq domain)
			{
				enc.ReserveInc(Token::Len + 1 + domain.n);
				Token::Generate(enc);
				enc.Ch('@');
				enc.Add(domain);
			}
		};


		struct MsgIdField
		{
			Seq m_id;

			MsgIdField() {}
			MsgIdField(Seq id) : m_id(id) {}

			void Read(ParseNode const& messageIdNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		struct SubjectField
		{
			Seq m_subject;

			SubjectField() {}
			SubjectField(Seq subject) : m_subject(subject) {}

			void Read(ParseNode const& subjectNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};


		enum class AutoSbmtdValue { Invalid = 0, No = 100, AutoGenerated = 200, AutoReplied = 300, Extension = 400 };

		struct AutoSbmtd : public Mime::FieldWithParams
		{
			AutoSbmtdValue m_value {};
			Seq m_extension;

			AutoSbmtd() {}
			AutoSbmtd(AutoSbmtdValue value) : m_value(value) {}

			void Read(ParseNode const& autoSbmtdFieldNode, PinStore& store);
			void Write(MsgWriter& writer) const;
		};



		// Message

		class Message : public Mime::Part
		{
		public:
			Seq mutable       m_hdrs;

			Opt<DateField>    m_date;
			Opt<FromField>    m_from;
			Opt<SenderField>  m_sender;
			Opt<ReplyToField> m_replyTo;
			Opt<ToField>      m_to;
			Opt<CcField>      m_cc;
			Opt<BccField>     m_bcc;
			Opt<MsgIdField>   m_msgId;
			Opt<SubjectField> m_subject;
			Opt<AutoSbmtd>    m_autoSbmtd;

			Mime::Part const* m_plainTextContentPart {};
			Mime::Part const* m_htmlContentPart      {};
			Mime::Part const* m_deliveryStatusPart   {};

			Vec<Mime::Part const*> m_htmlRelatedParts;
			Vec<Mime::Part const*> m_attachmentParts;

			Message() : Mime::Part(Mime::Part::Root) {}

			void ReadHeaders(ParseNode const& messageFieldsNode, PinStore& store);
			void WriteHeaders(MsgWriter& writer) const;

			// The Read() methods do not automatically read a multipart body, if there is one.
			// To read a multipart body (if there is one), follow up Read() with ReadMultipartBody().
			// If successful, updates m_hdrs to point to the part of input that contains message fields.
			bool Read(Seq message, PinStore& store);
			void Read(ParseTree const& ptMessage, PinStore& store);

			bool ReadMultipartBody(PinStore& store, Mime::PartReadCx& prcx);

			// Write() will produce a body from m_parts if IsMultipart() is true, and there are any parts.
			// If IsMultipart() is false, or there are no parts, it will write the verbatim body in m_content.
			// Updates m_hdrs to point to the part of output that contains message fields.
			void Write(MsgWriter& writer) const;

		private:
			void LocateParts();
			void LocateParts_Mixed       (Mime::Part const& mixPart);
			void LocateParts_Alternative (Mime::Part const& altPart);
			void LocateParts_Related     (Mime::Part const& relPart);
		};



		// Email addresses

		struct AddrSpecWithStore : AddrSpec { Opt<PinStore> m_pinStore; };

		bool ReadEmailAddressAsAddrSpec(Seq address, AddrSpecWithStore& addrSpec);
		bool ExtractLocalPartAndDomainFromEmailAddress(Seq address, Str* localPart, Str* domain);
		void NormalizeEmailAddress(ParseTree const& ptAddrSpec, PinStore& store, Str& normalized);
		bool NormalizeEmailAddress(Seq address, Str& normalized);
		bool NormalizeEmailAddressInPlace(Str& address);

		inline Str ExtractDomainFromEmailAddress(Seq address) { Str domain; if (!ExtractLocalPartAndDomainFromEmailAddress(address, 0, &domain)) return Str(); return domain; }


		struct ForEachAddressResult
		{
			sizet m_nrEnumerated {};
			Str   m_parseError;
		};

		// Enumerates email addresses separated by commas, semicolons, newlines, spaces, or even nothing (<aa@bb>cc@dd<ee@ff>).
		// Supports IMF addr-spec (user@example.com), IMF mailbox ("First Last" <user@example.com>), IMF group (group:user@example.com,other@example.com;)
		// For IMF group, each address is enumerated separately. If input can't be parsed, an error is returned and no addresses are enumerated.
		// Folding whitespace (with newlines) in email addresses is valid, and will be preserved if it's in a comment within the local part:
		// - "multiline\r\n @example.com" is rewritten as "multiline@example.com" (newline not preserved - not in comment)
		// - "multiline@(\r\n )example.com" is rewritten as "multiline@example.com" (newline not preserved - comments in domain part are dropped)
		// - "multiline(\r\n )@example.com" is preserved with newline included (newline preserved - comments in local part are kept)
		ForEachAddressResult ForEachAddressInCasualEmailAddressList(Seq casualAddressList, std::function<bool(Str&& addr)> f);



		// Domains

		bool IsValidDomain(Seq domain);
		bool IsEqualOrSubDomainOf(Seq exemplar, Seq other);		// Does not validate exemplar

	}  // namespace Imf
}
