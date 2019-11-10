#pragma once

#include "AtDsnGrammar.h"
#include "AtImfDateTime.h"
#include "AtTime.h"
#include "AtOpt.h"


namespace At
{
	namespace Dsn
	{

		// DSN fields

		enum class ActionValue { None, Failed, Delayed, Delivered, Relayed, Expanded };

		struct ActionField
		{
			ActionValue m_value;

			void Read(ParseNode const& field);
		};


		struct AtomTextField
		{
			Seq m_atom;
			Seq m_text;

			void Read(ParseNode const& field, PinStore& store);
		};


		struct AddrField
		{
			Seq m_atom;		// Empty if address provided in non-standard way as an angle-addr
			Seq m_addr;		// If address provided in non-standard way as an angle-addr, points to the inside addr-spec

			void Read(ParseNode const& field, PinStore& store);
		};


		struct DateTimeField
		{
			Imf::DateTime m_dt;

			void Read(ParseNode const& field);
		};


		struct StatusField
		{
			Seq m_statusCode;

			void Read(ParseNode const& field);
		};


		struct TextField
		{
			Seq m_text;

			void Read(ParseNode const& field, PinStore& store);
		};



		// DsnRecipient

		struct DsnRecipient
		{
			Opt<AddrField>     m_origRcpt;
			Opt<AddrField>     m_finalRcpt;
			Opt<ActionField>   m_action;
			Opt<StatusField>   m_status;
			Opt<AtomTextField> m_remoteMta;
			Opt<AtomTextField> m_diagCode;
			Opt<DateTimeField> m_lastAttempt;
			Opt<TextField>     m_finalLogId;
			Opt<DateTimeField> m_willRetryUntil;

			void Read(ParseNode const& fieldGroup, PinStore& store);
		};



		// DeliveryStatusNotification

		struct DeliveryStatusNotification
		{
			Opt<TextField>     m_origEnvlId;
			Opt<AtomTextField> m_reportingMta;
			Opt<AtomTextField> m_dsnGateway;
			Opt<AtomTextField> m_rcvdFromMta;
			Opt<DateTimeField> m_arrivalDate;

			Vec<DsnRecipient>  m_recipients;

			bool Read(Seq content, PinStore& store);
			void Read(ParseTree const& ptContent, PinStore& store);
		};

	}
}
