#include "AtIncludes.h"
#include "AtDsnGrammar.h"

#include "AtImfGrammar.h"


namespace At
{
	namespace Dsn
	{
		// Implements:
		// - RFC 3464: An Extensible Message Format for Delivery Status Notifications
		//
		// Extensions:
		// - Accepts fields in arbitrary order. Many implementations do actually send fields in a different order than specified.
		// - Accepts last field not terminated by CRLF. Some actual implementations send the last field not terminated by CRLF.
		// - Accepts per-recipient field groups not preceded by a per-message field group. Some actual implementations do not send a per-message field group.
		// - For email address fields, accepts "<address@example.com>" as alternative for "rfc822; address@example.com" because that's what some implementations send.
		// - For MTA address fields, accepts "1.2.3.4" as alternative for "dns; mx.example.com" because that's what some implementations send.

		using namespace Parse;
		using namespace Imf;

		DEF_RUID_B(action_value)
		DEF_RUID_X(av_failed,    id_action_value)
		DEF_RUID_X(av_delayed,   id_action_value)
		DEF_RUID_X(av_delivered, id_action_value)
		DEF_RUID_X(av_relayed,   id_action_value)
		DEF_RUID_X(av_expanded,  id_action_value)

		DEF_RUID_B(kw_action)
		DEF_RUID_B(kw_arrival_date)
		DEF_RUID_B(kw_diag_code)
		DEF_RUID_B(kw_dsn_gateway)
		DEF_RUID_B(kw_final_rcpt)
		DEF_RUID_B(kw_final_log_id)
		DEF_RUID_B(kw_last_attempt)
		DEF_RUID_B(kw_orig_envl_id)
		DEF_RUID_B(kw_orig_rcpt)
		DEF_RUID_B(kw_rcvd_from_mta)
		DEF_RUID_B(kw_remote_mta)
		DEF_RUID_B(kw_reporting_mta)
		DEF_RUID_B(kw_status)
		DEF_RUID_B(kw_will_retry)

		DEF_RUID_B(action)
		DEF_RUID_B(arrival_date)
		DEF_RUID_B(diag_code)
		DEF_RUID_B(dsn_gateway)
		DEF_RUID_B(ext_field_name)
		DEF_RUID_B(extension)
		DEF_RUID_B(final_rcpt)
		DEF_RUID_B(final_log_id)
		DEF_RUID_B(last_attempt)
		DEF_RUID_B(orig_envl_id)
		DEF_RUID_B(orig_rcpt)
		DEF_RUID_B(rcvd_from_mta)
		DEF_RUID_B(remote_mta)
		DEF_RUID_B(reporting_mta)
		DEF_RUID_B(status_code)
		DEF_RUID_B(status)
		DEF_RUID_B(will_retry_until)
		
		DEF_RUID_B(per_msg_fields)
		DEF_RUID_B(per_rcpt_fields)

		bool C_av_failed        (ParseNode& p) { return G_SeqMatchInsens   (p, id_av_failed,        "failed"               ); }
		bool C_av_delayed       (ParseNode& p) { return G_SeqMatchInsens   (p, id_av_delayed,       "delayed"              ); }
		bool C_av_delivered     (ParseNode& p) { return G_SeqMatchInsens   (p, id_av_delivered,     "delivered"            ); }
		bool C_av_relayed       (ParseNode& p) { return G_SeqMatchInsens   (p, id_av_relayed,       "relayed"              ); }
		bool C_av_expanded      (ParseNode& p) { return G_SeqMatchInsens   (p, id_av_expanded,      "expanded"             ); }

		// Make sure to exclude the names of any new fields with explicit parse rules from the generic C_ext_field_name
		bool C_kw_action        (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_action,        "Action"               ); }
		bool C_kw_arrival_date  (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_arrival_date,  "Arrival-Date"         ); }
		bool C_kw_diag_code     (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_diag_code,     "Diagnostic-Code"      ); }
		bool C_kw_dsn_gateway   (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_dsn_gateway,   "DSN-Gateway"          ); }
		bool C_kw_final_rcpt    (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_final_rcpt,    "Final-Recipient"      ); }
		bool C_kw_final_log_id  (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_final_log_id,  "Final-Log-ID"         ); }
		bool C_kw_last_attempt  (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_last_attempt,  "Last-Attempt-Date"    ); }
		bool C_kw_orig_envl_id  (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_orig_envl_id,  "Original-Envelope-Id" ); }
		bool C_kw_orig_rcpt     (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_orig_rcpt,     "Original-Recipient"   ); }
		bool C_kw_rcvd_from_mta (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_rcvd_from_mta, "Received-From-MTA"    ); }
		bool C_kw_remote_mta    (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_remote_mta,    "Remote-MTA"           ); }
		bool C_kw_reporting_mta (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_reporting_mta, "Reporting-MTA"        ); }
		bool C_kw_status        (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_status,        "Status"               ); }
		bool C_kw_will_retry    (ParseNode& p) { return G_SeqMatchInsens   (p, id_kw_will_retry,    "Will-Retry-Until"     ); }

		bool C_Colon_CFWS       (ParseNode& p) { return G_Req<1,0>         (p, id_Append,            C_Colon, C_CFWS                                                                                            ); }
		bool C_Semicolon_CFWS   (ParseNode& p) { return G_Req<0,1,0>       (p, id_Append,            C_CFWS, C_Semicolon, C_CFWS                                                                                ); }

		bool C_ext_field_name   (ParseNode& p);

		bool C_dsn_type_text    (ParseNode& p) { return G_Req<1,1,1>       (p, id_Append,            C_atext_word, C_Semicolon_CFWS, Imf::C_unstructured                                                        ); }
		bool C_dsn_addr         (ParseNode& p) { return G_Choice           (p, id_Append,            C_dsn_type_text, C_angle_addr                                                                              ); }
		bool C_dsn_mta          (ParseNode& p) { return G_Choice           (p, id_Append,            C_dsn_type_text, Imf::C_unstructured                                                                       ); }

		bool C_action_value     (ParseNode& p) { return G_Choice           (p, id_Append,            C_av_failed, C_av_delayed, C_av_delivered, C_av_relayed, C_av_expanded                                     ); }
		bool C_action           (ParseNode& p) { return G_Req<1,1,1,0>     (p, id_action,            C_kw_action, C_Colon_CFWS, C_action_value, C_CFWS                                                          ); }
		bool C_arrival_date     (ParseNode& p) { return G_Req<1,1,1>       (p, id_arrival_date,      C_kw_arrival_date, C_Colon_CFWS, C_date_time                                                               ); }
		bool C_diag_code        (ParseNode& p) { return G_Req<1,1,1>       (p, id_diag_code,         C_kw_diag_code, C_Colon_CFWS, C_dsn_type_text                                                              ); }
		bool C_dsn_gateway      (ParseNode& p) { return G_Req<1,1,1>       (p, id_dsn_gateway,       C_kw_dsn_gateway, C_Colon_CFWS, C_dsn_mta                                                                  ); }
		bool C_extension        (ParseNode& p) { return G_Req<1,1,1>       (p, id_extension,         C_ext_field_name, C_Colon_CFWS, Imf::C_unstructured                                                        ); }
		bool C_final_rcpt       (ParseNode& p) { return G_Req<1,1,1>       (p, id_final_rcpt,        C_kw_final_rcpt, C_Colon_CFWS, C_dsn_addr                                                                  ); }
		bool C_final_log_id     (ParseNode& p) { return G_Req<1,1,1>       (p, id_final_log_id,      C_kw_final_log_id, C_Colon_CFWS, Imf::C_unstructured                                                       ); }
		bool C_last_attempt     (ParseNode& p) { return G_Req<1,1,1>       (p, id_last_attempt,      C_kw_last_attempt, C_Colon_CFWS, C_date_time                                                               ); }
		bool C_orig_envl_id     (ParseNode& p) { return G_Req<1,1,1>       (p, id_orig_envl_id,      C_kw_orig_envl_id, C_Colon_CFWS, Imf::C_unstructured                                                       ); }
		bool C_orig_rcpt        (ParseNode& p) { return G_Req<1,1,1>       (p, id_orig_rcpt,         C_kw_orig_rcpt, C_Colon_CFWS, C_dsn_addr                                                                   ); }
		bool C_rcvd_from_mta    (ParseNode& p) { return G_Req<1,1,1>       (p, id_rcvd_from_mta,     C_kw_rcvd_from_mta, C_Colon_CFWS, C_dsn_mta                                                                ); }
		bool C_remote_mta       (ParseNode& p) { return G_Req<1,1,1>       (p, id_remote_mta,        C_kw_remote_mta, C_Colon_CFWS, C_dsn_mta                                                                   ); }
		bool C_reporting_mta    (ParseNode& p) { return G_Req<1,1,1>       (p, id_reporting_mta,     C_kw_reporting_mta, C_Colon_CFWS, C_dsn_mta                                                                ); }
		bool C_status_code      (ParseNode& p) { return G_Req<1,1,1,1,1>   (p, id_status_code,       V_AsciiDecDigit, V_Dot, G_Repeat<V_AsciiDecDigit,1,3>, V_Dot, G_Repeat<V_AsciiDecDigit,1,3>                ); }
		bool C_status           (ParseNode& p) { return G_Req<1,1,1,0>     (p, id_status,            C_kw_status, C_Colon_CFWS, C_status_code, C_CFWS                                                           ); }
		bool C_will_retry_until (ParseNode& p) { return G_Req<1,1,1>       (p, id_will_retry_until,  C_kw_will_retry, C_Colon_CFWS, C_date_time                                                                 ); }

		bool C_per_msg_field    (ParseNode& p) { return G_Choice           (p, id_Append,            C_orig_envl_id, C_reporting_mta, C_dsn_gateway, C_rcvd_from_mta, C_arrival_date, C_extension               ); }
		bool C_per_msg_group    (ParseNode& p) { return G_Repeat           (p, id_per_msg_fields,    G_Req<C_per_msg_field, G_Choice<C_CRLF, N_End>>                                                            ); }
		bool C_per_rcpt_field_1 (ParseNode& p) { return G_Choice           (p, id_Append,            C_orig_rcpt, C_final_rcpt, C_action, C_status, C_remote_mta                                                ); }
		bool C_per_rcpt_field_2 (ParseNode& p) { return G_Choice           (p, id_Append,            C_diag_code, C_last_attempt, C_final_log_id, C_will_retry_until, C_extension                               ); }
		bool C_per_rcpt_group   (ParseNode& p) { return G_Repeat           (p, id_per_rcpt_fields,   G_Req<G_Choice<C_per_rcpt_field_1, C_per_rcpt_field_2>, G_Choice<C_CRLF, N_End>>                           ); }
		bool C_first_group      (ParseNode& p) { return G_Choice           (p, id_Append,            C_per_msg_group, C_per_rcpt_group                                                                          ); }
		bool C_subsequent_group (ParseNode& p) { return G_Req<1,1>         (p, id_Append,            C_CRLF, C_per_rcpt_group                                                                                   ); }
		bool C_dsn_content      (ParseNode& p) { return G_Req<1,0,0>       (p, id_Append,            C_first_group, G_Repeat<C_subsequent_group>, G_Repeat<G_Choice<C_ImfWs, C_CRLF>>                           ); }


		bool C_ext_field_name(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_ext_field_name);
			if (!pn)
				return false;

			if (!C_atext_word(*pn))
				return p.FailChild(pn);

			// To match this rule, must NOT be a known field name
			if (pn->SrcText().EqualInsensitive("Action"               ) ||
				pn->SrcText().EqualInsensitive("Arrival-Date"         ) ||
				pn->SrcText().EqualInsensitive("Diagnostic-Code"      ) ||
				pn->SrcText().EqualInsensitive("DSN-Gateway"          ) ||
				pn->SrcText().EqualInsensitive("Final-Recipient"      ) ||
				pn->SrcText().EqualInsensitive("Final-Log-ID"         ) ||
				pn->SrcText().EqualInsensitive("Last-Attempt-Date"    ) ||
				pn->SrcText().EqualInsensitive("Original-Envelope-Id" ) ||
				pn->SrcText().EqualInsensitive("Original-Recipient"   ) ||
				pn->SrcText().EqualInsensitive("Received-From-MTA"    ) ||
				pn->SrcText().EqualInsensitive("Remote-MTA"           ) ||
				pn->SrcText().EqualInsensitive("Reporting-MTA"        ) ||
				pn->SrcText().EqualInsensitive("Status"               ) ||
				pn->SrcText().EqualInsensitive("Will-Retry-Until"     ))
			{
				return p.FailChild(pn);
			}

			return p.CommitChild(pn);
		}

	}
}
