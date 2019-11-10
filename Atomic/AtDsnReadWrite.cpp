#include "AtIncludes.h"
#include "AtDsnReadWrite.h"

#include "AtImfReadWrite.h"


namespace At
{
	namespace Dsn
	{

		// DSN fields

		void ActionField::Read(ParseNode const& field)
		{
			ParseNode const& value = field.FlatFindRef(id_action_value);
			     if (value.IsType(id_av_failed    )) m_value = ActionValue::Failed;
			else if (value.IsType(id_av_delayed   )) m_value = ActionValue::Delayed;
			else if (value.IsType(id_av_delivered )) m_value = ActionValue::Delivered;
			else if (value.IsType(id_av_relayed   )) m_value = ActionValue::Relayed;
			else if (value.IsType(id_av_expanded  )) m_value = ActionValue::Expanded;
		}


		void AtomTextField::Read(ParseNode const& field, PinStore& store)
		{
			for (ParseNode const& elem : field)
				     if (elem.IsType(Imf::id_atext_word))   m_atom = elem.SrcText();
				else if (elem.IsType(Imf::id_unstructured)) m_text = Imf::ReadUnstructured(elem, store);
		}


		void AddrField::Read(ParseNode const& field, PinStore& store)
		{
			for (ParseNode const& elem : field)
				     if (elem.IsType(Imf::id_atext_word))   m_atom = elem.SrcText();
				else if (elem.IsType(Imf::id_unstructured)) m_addr = Imf::ReadUnstructured(elem, store);
				else if (elem.IsType(Imf::id_angle_addr))   m_addr = elem.FlatFindRef(Imf::id_addr_spec).SrcText();
		}


		void DateTimeField::Read(ParseNode const& field)
		{
			m_dt.Read(field.FlatFindRef(Imf::id_date_time));
		}


		void StatusField::Read(ParseNode const& field)
		{
			m_statusCode = field.FlatFindRef(id_status_code).SrcText();
		}


		void TextField::Read(ParseNode const& field, PinStore& store)
		{
			ParseNode const* unstructured = field.FlatFind(Imf::id_unstructured);
			if (unstructured)
				m_text = Imf::ReadUnstructured(*unstructured, store);
		}



		// DsnRecipient

		void DsnRecipient::Read(ParseNode const& fieldGroup, PinStore& store)
		{
			for (ParseNode const& field : fieldGroup)
			{
				     if (field.IsType(id_orig_rcpt        )) { m_origRcpt       .ReInit().Read(field, store ); }
				else if (field.IsType(id_final_rcpt       )) { m_finalRcpt      .ReInit().Read(field, store ); }
				else if (field.IsType(id_action           )) { m_action         .ReInit().Read(field        ); }
				else if (field.IsType(id_status           )) { m_status         .ReInit().Read(field        ); }
				else if (field.IsType(id_remote_mta       )) { m_remoteMta      .ReInit().Read(field, store ); }
				else if (field.IsType(id_diag_code        )) { m_diagCode       .ReInit().Read(field, store ); }
				else if (field.IsType(id_last_attempt     )) { m_lastAttempt    .ReInit().Read(field        ); }
				else if (field.IsType(id_final_log_id     )) { m_finalLogId     .ReInit().Read(field, store ); }
				else if (field.IsType(id_will_retry_until )) { m_willRetryUntil .ReInit().Read(field        ); }
			}
		}



		// DeliveryStatusNotification

		bool DeliveryStatusNotification::Read(Seq content, PinStore& store)
		{
			ParseTree pt { content };
			if (!pt.Parse(C_dsn_content))
				return false;

			Read(pt, store);
			return true;
		}


		void DeliveryStatusNotification::Read(ParseTree const& ptContent, PinStore& store)
		{
			for (ParseNode const& fieldGroup : ptContent.Root())
				if (fieldGroup.IsType(id_per_msg_fields))
				{
					for (ParseNode const& field : fieldGroup)
					{
						     if (field.IsType(id_orig_envl_id  )) { m_origEnvlId  .ReInit().Read(field, store ); }
						else if (field.IsType(id_reporting_mta )) { m_reportingMta.ReInit().Read(field, store ); }
						else if (field.IsType(id_dsn_gateway   )) { m_dsnGateway  .ReInit().Read(field, store ); }
						else if (field.IsType(id_rcvd_from_mta )) { m_rcvdFromMta .ReInit().Read(field, store ); }
						else if (field.IsType(id_arrival_date  )) { m_arrivalDate .ReInit().Read(field        ); }
					}
				}
				else if (fieldGroup.IsType(id_per_rcpt_fields))
					m_recipients.Add().Read(fieldGroup, store);
		}

	}
}
