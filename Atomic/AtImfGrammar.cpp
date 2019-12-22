#include "AtIncludes.h"
#include "AtImfGrammar.h"

#include "AtMimeGrammar.h"
#include "AtSmtpGrammar.h"


namespace At
{
	namespace Imf
	{
		using namespace Parse;

		DEF_RUID_B(AcceptObs)

		// This flag needs to be checked at fork spots - i.e. at parsers for obsolete forms which can be referenced by parsers for non-obsolete forms.
		// Parsers for obsolete forms that are referenced only by other parsers for obsolete forms do not need to check this.

		namespace
		{
			inline bool Obs(ParseNode const& p) { return p.Tree().HaveFlag(id_AcceptObs); }
		}


		// This grammar implements RFC 5322, including relevant errata as of 2014-09-12.
		// 
		// - With regard to Errata ID 1905, regarding the definition of 'obs-unstruct', the solution implemented here is to define C_obs_utext_word to
		//   consume any character sequence except HTAB; SP; or CR when followed by LF. Then, we allow C_obs_utext_word instead of C_std_utext_word.
		//
		// - With regard to Errata ID 1908, regarding the definition of 'obs-FWS', implemented as specified.
		//
		// - With regard to Errata ID 3979, regarding the definition of 'received' and 'obs-received', implemented as specified.
		//
		// Changes specific to this implementation:
		//
		// - We modify 'obs-received' to allow 'received-token' being followed by 'date-time', so that 'obs-received' is a proper superset of 'received'.
		//
		// - We modify 'trace', as implemented by C_trace_field_grp, to allow for 'optional-field' between 'return' and 'received'.
		//
		// - We modify 'obs-fields' so that each field within 'obs-fields' can also be parsed according to its standard definition, reverting to the
		//   obsolete definition only if parsing according to the standard definition fails.

		// Also implemented:
		// RFC 2047 - MIME (Multipurpose Internet Mail Extensions) Part Three: Message Header Extensions for Non-ASCII Text
		// RFC 3834 - Recommendations for Automatic Responses to Electronic Mail
		// RFC 6532 - Internationalized Email Headers
		// RFC 6854 - Update to Internet Message Format to Allow Group Syntax in the "From:" and "Sender:" Header Fields

		inline bool Is_WSP_byte           (uint c) { return c == 9 || c == 32; }
		inline bool Is_obs_NO_WS_CTL_byte (uint c) { return (c >= 1 && c <= 8) || c == 11 || c == 12 || (c >= 14 && c <= 31) || (c == 127); }
		inline bool Is_obs_qp_byte        (uint c) { return c <= 31 || c == 127; }
		inline bool Is_mil_zone_byte      (uint c) { return (c >= 65 && c <= 73) || (c >= 75 && c <= 90) || (c >= 97 && c <= 105) || (c >= 107 && c <= 122); }
		inline bool Is_field_name_byte    (uint c) { return (c >= 33 && c <= 57) || (c >= 59 && c <= 126); }
										  
		inline bool Is_VCHAR              (uint c) { return (c >= 33 && c != 127); }
		inline bool Is_quoted_pair_char   (uint c) { return Is_VCHAR(c) || Is_WSP_byte(c); }
		inline bool Is_std_ctext_char     (uint c) { return (c >= 33 && c <= 39) || (c >= 42 && c <= 91) || (c >= 93 && c != 127); }
		inline bool Is_dtext_char         (uint c) { return (c >= 33 && c <= 90) || (c >= 94 && c != 127); }

		bool V_WSP_byte           (ParseNode& p) { return V_ByteIf        (p, Is_WSP_byte); }
		bool V_obs_NO_WS_CTL_byte (ParseNode& p) { return V_ByteIf        (p, Is_obs_NO_WS_CTL_byte); }
		bool V_obs_qp_byte        (ParseNode& p) { return V_ByteIf        (p, Is_obs_qp_byte); }
		bool V_field_name_byte    (ParseNode& p) { return V_ByteIf        (p, Is_field_name_byte); }
																		  
		bool V_VCHAR              (ParseNode& p) { return V_Utf8CharIf    (p, Is_VCHAR); }
		bool V_quoted_pair_char   (ParseNode& p) { return V_Utf8CharIf    (p, Is_quoted_pair_char); }
		bool V_dtext_char         (ParseNode& p) { return V_Utf8CharIf    (p, Is_dtext_char); }
																		  
		bool V_ImfWs              (ParseNode& p) { return G_Repeat        (p, id_Append, V_WSP_byte); }
		bool V_OptWs_CRLF         (ParseNode& p) { return G_Req<0,1>      (p, id_Append, V_ImfWs, V_CRLF); }
		bool V_std_FWS            (ParseNode& p) { return G_Req<0,1>      (p, id_Append, V_OptWs_CRLF, V_ImfWs); }
		bool V_std_dtext          (ParseNode& p) { return G_Repeat        (p, id_Append, V_dtext_char); }
		bool V_field_name         (ParseNode& p) { return G_Repeat        (p, id_Append, V_field_name_byte); }

		bool V_dkim_alnumpunc     (ParseNode& p) { return V_ByteIf        (p, [] (uint c) -> bool { return Ascii::IsAlphaNum(c) || c == '_'; } ); }
		bool V_dkim_valchar       (ParseNode& p) { return V_Utf8CharIf    (p, [] (uint c) -> bool { return (c >= 0x21 && c <= 0x3A) || (c >= 0x3C && c != 0x7F); } ); }
		bool V_dkim_valpart       (ParseNode& p) { return G_Repeat        (p, id_Append, V_dkim_valchar); }
		bool V_dkim_tag_name      (ParseNode& p) { return G_Req<1,0>      (p, id_Append, V_AsciiAlpha, G_Repeat<V_dkim_alnumpunc> ); }
		bool V_dkim_tag_value     (ParseNode& p) { return G_Req<1,0>      (p, id_Append, V_dkim_valpart, G_Repeat<G_Req<V_std_FWS, V_dkim_valpart>> ); }

		bool V_kw_Mon             (ParseNode& p) { return V_SeqMatchInsens(p, "Mon"); }
		bool V_kw_Tue             (ParseNode& p) { return V_SeqMatchInsens(p, "Tue"); }
		bool V_kw_Wed             (ParseNode& p) { return V_SeqMatchInsens(p, "Wed"); }
		bool V_kw_Thu             (ParseNode& p) { return V_SeqMatchInsens(p, "Thu"); }
		bool V_kw_Fri             (ParseNode& p) { return V_SeqMatchInsens(p, "Fri"); }
		bool V_kw_Sat             (ParseNode& p) { return V_SeqMatchInsens(p, "Sat"); }
		bool V_kw_Sun             (ParseNode& p) { return V_SeqMatchInsens(p, "Sun"); }
		bool V_day_name           (ParseNode& p) { return G_Choice(p, id_Append, V_kw_Mon, V_kw_Tue, V_kw_Wed, V_kw_Thu, V_kw_Fri, V_kw_Sat, V_kw_Sun); }

		bool V_kw_Jan             (ParseNode& p) { return V_SeqMatchInsens(p, "Jan"); }
		bool V_kw_Feb             (ParseNode& p) { return V_SeqMatchInsens(p, "Feb"); }
		bool V_kw_Mar             (ParseNode& p) { return V_SeqMatchInsens(p, "Mar"); }
		bool V_kw_Apr             (ParseNode& p) { return V_SeqMatchInsens(p, "Apr"); }
		bool V_kw_May             (ParseNode& p) { return V_SeqMatchInsens(p, "May"); }
		bool V_kw_Jun             (ParseNode& p) { return V_SeqMatchInsens(p, "Jun"); }
		bool V_kw_Jul             (ParseNode& p) { return V_SeqMatchInsens(p, "Jul"); }
		bool V_kw_Aug             (ParseNode& p) { return V_SeqMatchInsens(p, "Aug"); }
		bool V_kw_Sep             (ParseNode& p) { return V_SeqMatchInsens(p, "Sep"); }
		bool V_kw_Oct             (ParseNode& p) { return V_SeqMatchInsens(p, "Oct"); }
		bool V_kw_Nov             (ParseNode& p) { return V_SeqMatchInsens(p, "Nov"); }
		bool V_kw_Dec             (ParseNode& p) { return V_SeqMatchInsens(p, "Dec"); }
		bool V_month              (ParseNode& p) { return G_Choice(p, id_Append, V_kw_Jan, V_kw_Feb, V_kw_Mar, V_kw_Apr, V_kw_May, V_kw_Jun,
		                                                                         V_kw_Jul, V_kw_Aug, V_kw_Sep, V_kw_Oct, V_kw_Nov, V_kw_Dec); }

		bool V_kw_UT              (ParseNode& p) { return V_SeqMatchInsens(p, "UT"); }
		bool V_kw_GMT             (ParseNode& p) { return V_SeqMatchInsens(p, "GMT"); }
		bool V_kw_EST             (ParseNode& p) { return V_SeqMatchInsens(p, "EST"); }
		bool V_kw_EDT             (ParseNode& p) { return V_SeqMatchInsens(p, "EDT"); }
		bool V_kw_CST             (ParseNode& p) { return V_SeqMatchInsens(p, "CST"); }
		bool V_kw_CDT             (ParseNode& p) { return V_SeqMatchInsens(p, "CDT"); }
		bool V_kw_MST             (ParseNode& p) { return V_SeqMatchInsens(p, "MST"); }
		bool V_kw_MDT             (ParseNode& p) { return V_SeqMatchInsens(p, "MDT"); }
		bool V_kw_PST             (ParseNode& p) { return V_SeqMatchInsens(p, "PST"); }
		bool V_kw_PDT             (ParseNode& p) { return V_SeqMatchInsens(p, "PDT"); }
		bool V_mil_zone           (ParseNode& p) { return V_ByteIf(p, Is_mil_zone_byte); }
		bool V_obs_zone           (ParseNode& p) { return G_Choice(p, id_Append, V_kw_UT, V_kw_GMT, V_kw_EST, V_kw_EDT, V_kw_CST, V_kw_CDT,
		                                                                         V_kw_MST, V_kw_MDT, V_kw_PST, V_kw_PDT, V_mil_zone); }

		bool V_1or2digits         (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiDecDigit, 1, 2); }
		bool V_2digits            (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiDecDigit, 2, 2); }
		bool V_2plusdigits        (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiDecDigit, 2); }
		bool V_4digits            (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiDecDigit, 4, 4); }
		bool V_4plusdigits        (ParseNode& p) { return G_Repeat(p, id_Append, V_AsciiDecDigit, 4); }

		bool V_kw_no              (ParseNode& p) { return V_SeqMatchInsens(p, "no"); }
		bool V_kw_auto_gen        (ParseNode& p) { return V_SeqMatchInsens(p, "auto-generated"); }
		bool V_kw_auto_repl       (ParseNode& p) { return V_SeqMatchInsens(p, "auto-replied"); }

		// Make sure to exclude the names of any new fields with explicit parse rules from the generic C_opt_field_name
		bool V_kw_return          (ParseNode& p) { return V_SeqMatchInsens(p, "Return-Path"); }
		bool V_kw_received        (ParseNode& p) { return V_SeqMatchInsens(p, "Received"); }
		bool V_kw_resent_date     (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Date"); }
		bool V_kw_resent_from     (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-From"); }
		bool V_kw_resent_sender   (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Sender"); }
		bool V_kw_resent_to       (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-To"); }
		bool V_kw_resent_cc       (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Cc"); }
		bool V_kw_resent_bcc      (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Bcc"); }
		bool V_kw_resent_msg_id   (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Message-ID"); }
		bool V_kw_resent_rply     (ParseNode& p) { return V_SeqMatchInsens(p, "Resent-Reply-To"); }
		bool V_kw_date            (ParseNode& p) { return V_SeqMatchInsens(p, "Date"); }
		bool V_kw_from            (ParseNode& p) { return V_SeqMatchInsens(p, "From"); }
		bool V_kw_sender          (ParseNode& p) { return V_SeqMatchInsens(p, "Sender"); }
		bool V_kw_reply_to        (ParseNode& p) { return V_SeqMatchInsens(p, "Reply-To"); }
		bool V_kw_to              (ParseNode& p) { return V_SeqMatchInsens(p, "To"); }
		bool V_kw_cc              (ParseNode& p) { return V_SeqMatchInsens(p, "Cc"); }
		bool V_kw_bcc             (ParseNode& p) { return V_SeqMatchInsens(p, "Bcc"); }
		bool V_kw_message_id      (ParseNode& p) { return V_SeqMatchInsens(p, "Message-ID"); }
		bool V_kw_in_reply_to     (ParseNode& p) { return V_SeqMatchInsens(p, "In-Reply-To"); }
		bool V_kw_references      (ParseNode& p) { return V_SeqMatchInsens(p, "References"); }
		bool V_kw_subject         (ParseNode& p) { return V_SeqMatchInsens(p, "Subject"); }
		bool V_kw_comments        (ParseNode& p) { return V_SeqMatchInsens(p, "Comments"); }
		bool V_kw_keywords        (ParseNode& p) { return V_SeqMatchInsens(p, "Keywords"); }
		bool V_kw_dkim_sig        (ParseNode& p) { return V_SeqMatchInsens(p, "DKIM-Signature"); }
		bool V_kw_auto_sbmtd      (ParseNode& p) { return V_SeqMatchInsens(p, "Auto-Submitted"); }

		DEF_RUID_B(ImfWs)

		DEF_RUID_B(FWS)
		DEF_RUID_B(std_dtext)
		DEF_RUID_B(obs_dtext_char)
		DEF_RUID_B(no_fold_literal)

		DEF_RUID_B(field_name)

		DEF_RUID_B(encw_start)
		DEF_RUID_B(encw_end)
		DEF_RUID_B(encw_charset)
		DEF_RUID_B(encw_encoding)
		DEF_RUID_B(encoded_text)
		DEF_RUID_B(encoded_word)
		DEF_RUID_B(encw_group)

		DEF_RUID_B(atext_word)
		DEF_RUID_B(dot_atom_text)
		DEF_RUID_B(phrase)
		DEF_RUID_B(phrase_list)

		DEF_RUID_B(utext_word)
		DEF_RUID_B(unstructured)

		DEF_RUID_B(quoted_pair)

		DEF_RUID_B(ctext_word)
		DEF_RUID_B(ccontent)
		DEF_RUID_B(comment)
		DEF_RUID_B(CFWS)

		DEF_RUID_B(qtext_word)
		DEF_RUID_B(qs_content)
		DEF_RUID_B(quoted_string)

		DEF_RUID_B(local_part)
		DEF_RUID_B(domain)
		DEF_RUID_B(addr_spec)

		DEF_RUID_B(angle_addr)
		DEF_RUID_B(obs_domain_list)
		DEF_RUID_B(name_addr)
		DEF_RUID_B(mailbox)

		DEF_RUID_B(mailbox_list)
		DEF_RUID_B(group)
		DEF_RUID_B(address)
		DEF_RUID_B(casual_addr_seps)
		DEF_RUID_B(address_list)

		DEF_RUID_B(2digits)
		DEF_RUID_B(4digits)

		DEF_RUID_B(day_name)
		DEF_RUID_B(day_digits)
		DEF_RUID_B(day)
		DEF_RUID_B(month)
		DEF_RUID_B(year_digits)
		DEF_RUID_B(year)
		DEF_RUID_B(date)
		DEF_RUID_B(zone)
		DEF_RUID_B(time)
		DEF_RUID_B(date_time)

		DEF_RUID_B(msg_id)

		DEF_RUID_B(kw_no)
		DEF_RUID_B(kw_auto_gen)
		DEF_RUID_B(kw_auto_repl)

		DEF_RUID_X(kw_return,        id_field_name)
		DEF_RUID_X(kw_received,      id_field_name)
		DEF_RUID_X(kw_resent_date,   id_field_name)
		DEF_RUID_X(kw_resent_from,   id_field_name)
		DEF_RUID_X(kw_resent_sender, id_field_name)
		DEF_RUID_X(kw_resent_to,     id_field_name)
		DEF_RUID_X(kw_resent_cc,     id_field_name)
		DEF_RUID_X(kw_resent_bcc,    id_field_name)
		DEF_RUID_X(kw_resent_msg_id, id_field_name)
		DEF_RUID_X(kw_resent_rply,   id_field_name)
		DEF_RUID_X(kw_date,          id_field_name)
		DEF_RUID_X(kw_from,          id_field_name)
		DEF_RUID_X(kw_sender,        id_field_name)
		DEF_RUID_X(kw_reply_to,      id_field_name)
		DEF_RUID_X(kw_to,            id_field_name)
		DEF_RUID_X(kw_cc,            id_field_name)
		DEF_RUID_X(kw_bcc,           id_field_name)
		DEF_RUID_X(kw_message_id,    id_field_name)
		DEF_RUID_X(kw_in_reply_to,   id_field_name)
		DEF_RUID_X(kw_references,    id_field_name)
		DEF_RUID_X(kw_subject,       id_field_name)
		DEF_RUID_X(kw_comments,      id_field_name)
		DEF_RUID_X(kw_keywords,      id_field_name)
		DEF_RUID_X(kw_dkim_sig,      id_field_name)
		DEF_RUID_X(kw_auto_sbmtd,    id_field_name)

		DEF_RUID_B(path)
		DEF_RUID_B(return)
		DEF_RUID_B(received)
		DEF_RUID_B(obs_received)
		DEF_RUID_B(trace_fields)

		DEF_RUID_B(resent_date)
		DEF_RUID_B(resent_from)
		DEF_RUID_B(resent_sender)
		DEF_RUID_B(resent_to)
		DEF_RUID_B(resent_cc)
		DEF_RUID_B(resent_bcc)
		DEF_RUID_B(resent_msg_id)
		DEF_RUID_B(resent_rply)
		DEF_RUID_B(orig_date)

		DEF_RUID_B(from)
		DEF_RUID_B(sender)
		DEF_RUID_B(reply_to)
		DEF_RUID_B(to)
		DEF_RUID_B(cc)
		DEF_RUID_B(bcc)
		DEF_RUID_B(message_id)
		DEF_RUID_B(in_reply_to)
		DEF_RUID_B(references)
		DEF_RUID_B(subject)
		DEF_RUID_B(comments)
		DEF_RUID_B(keywords)

		DEF_RUID_B(dkim_hdr_list)
		DEF_RUID_B(dkim_auid)
		DEF_RUID_B(dkim_tag_name)
		DEF_RUID_B(dkim_tag_value)
		DEF_RUID_B(dkim_tag)
		DEF_RUID_B(dkim_tag_list)
		DEF_RUID_B(dkim_signature)

		DEF_RUID_B(auto_sbmtd_value)
		DEF_RUID_B(auto_sbmtd_field)

		DEF_RUID_X(opt_field_name, id_field_name)
		DEF_RUID_B(optional_field)
		DEF_RUID_X(inv_field_name, id_field_name)
		DEF_RUID_B(invalid_field)
		DEF_RUID_B(tr_resent_fields)
		DEF_RUID_B(main_fields)

		DEF_RUID_B(body)
		DEF_RUID_B(message_fields)
		DEF_RUID_B(message);

		bool C_ImfWs            (ParseNode& p) { return           G_Req                (p, id_ImfWs,            V_ImfWs                                                                                            ); }
																					  																														     
		bool C_std_FWS          (ParseNode& p) { return           G_Req                (p, id_FWS,              V_std_FWS                                                                                          ); }
		bool C_obs_FWS          (ParseNode& p) { return Obs(p) && G_Req<0,1>           (p, id_FWS,              V_ImfWs, G_Repeat<G_Req<V_CRLF, V_ImfWs>>                                                          ); }
		bool C_FWS              (ParseNode& p) { return           G_Choice             (p, id_Append,           C_obs_FWS, C_std_FWS                                                                               ); }
																					  																														     
		bool C_encw_start       (ParseNode& p) { return           G_SeqMatchExact      (p, id_encw_start,       "=?"                                                                                               ); }
		bool C_encw_end         (ParseNode& p) { return           G_SeqMatchExact      (p, id_encw_start,       "?="                                                                                               ); }
		bool V_encw_token_byte  (ParseNode& p) { return           V_ByteIf             (p,                      [] (uint c) -> bool { return c>=33 && c<=126 && !ZChr("()<>@,;:\"/[]?.=", c); }                    ); }
		bool V_encw_token       (ParseNode& p) { return           G_Repeat             (p, id_Append,           V_encw_token_byte                                                                                  ); }
		bool C_encw_charset     (ParseNode& p) { return           G_Req                (p, id_encw_charset,     V_encw_token                                                                                       ); }
		bool C_encw_encoding    (ParseNode& p) { return           G_Req                (p, id_encw_encoding,    V_encw_token                                                                                       ); }
		bool V_encw_text_byte   (ParseNode& p) { return           V_ByteIf             (p,                      [] (uint c) -> bool { return c>=33 && c<=126 && c!='?'; }                                          ); }
		bool C_encoded_text     (ParseNode& p) { return           G_Repeat             (p, id_encoded_text,     G_Repeat<V_encw_text_byte>                                                                         ); }
		bool C_encw_inner       (ParseNode& p) { return           G_Req<1,1,1,1,0>     (p, id_Append,           C_encw_charset, C_QuestnMark, C_encw_encoding, C_QuestnMark, C_encoded_text                        ); } 
		bool C_encoded_word     (ParseNode& p) { return           G_Req<1,1,1>         (p, id_encoded_word,     C_encw_start, C_encw_inner, C_encw_end                                                             ); }
		bool C_encw_group       (ParseNode& p) { return           G_Req<1,0>           (p, id_encw_group,       C_encoded_word, G_Repeat<G_Req<C_FWS, C_encoded_word>>                                             ); }
																					  																														     
		bool C_std_dtext        (ParseNode& p) { return           G_Req                (p, id_std_dtext,        V_std_dtext                                                                                        ); }
		bool C_obs_dtext_byte   (ParseNode& p) { return           G_Req                (p, id_obs_dtext_char,   V_obs_NO_WS_CTL_byte                                                                               ); }
		bool C_obs_dtext_elem   (ParseNode& p) { return Obs(p) && G_Choice             (p, id_Append,           C_obs_dtext_byte, C_quoted_pair                                                                    ); }
		bool C_dtext_elem       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_dtext, C_obs_dtext_elem                                                                      ); }
		bool C_FWS_dtext        (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, C_dtext_elem                                                                                ); }
		bool C_no_fold_literal  (ParseNode& p) { return           G_Req<1,1,1>         (p, id_no_fold_literal,  C_SqOpenBr, G_Repeat<C_dtext_elem>, C_SqCloseBr                                                    ); }

		bool C_field_name       (ParseNode& p) { return           G_Req                (p, id_field_name,       V_field_name                                                                                       ); }

		bool V_atext_char       (ParseNode& p) { return           V_Utf8CharIf         (p,                      [] (uint c) -> bool { return Ascii::IsAlphaNum(c) || (c>=128) || ZChr("!#$%&'*+-/=?^_`{|}~", c); } ); }
		bool C_atext_word       (ParseNode& p) { return           G_Repeat             (p, id_atext_word,       V_atext_char                                                                                       ); }
		bool C_atom             (ParseNode& p) { return           G_Req<0,1,0>         (p, id_Append,           C_CFWS, C_atext_word, C_CFWS                                                                       ); }
		bool C_dot_atom_text    (ParseNode& p) { return           G_Req<1,0>           (p, id_dot_atom_text,    C_atext_word, G_Repeat<G_Req<C_Dot, C_atext_word>>                                                 ); }
		bool C_dot_atom         (ParseNode& p) { return           G_Req<0,1,0>         (p, id_Append,           C_CFWS, C_dot_atom_text, C_CFWS                                                                    ); }
		bool C_word             (ParseNode& p) { return           G_Choice             (p, id_Append,           C_atom, C_quoted_string                                                                            ); }
																					  																														     
		bool C_obs_phrase_el    (ParseNode& p) { return Obs(p) && G_Req                (p, id_Append,           C_Dot                                                                                              ); }
		bool C_std_phrase_el    (ParseNode& p) { return           G_Choice             (p, id_Append,           C_encw_group, C_word                                                                               ); }
		bool C_phrase           (ParseNode& p) { return           G_Req<1,0>           (p, id_phrase,           C_std_phrase_el, G_Repeat<G_Choice<C_std_phrase_el, C_obs_phrase_el>>                              ); }
																					  																														     
		bool C_std_phrase_list  (ParseNode& p) { return           G_Req<1,0>           (p, id_phrase_list,      C_phrase, G_Repeat<G_Req<C_Comma, C_phrase>>                                                       ); }
		bool C_phrase_or_CFWS   (ParseNode& p) { return           G_Choice             (p, id_Append,           C_phrase, C_CFWS                                                                                   ); }
		bool C_obs_phrase_list  (ParseNode& p) { return           G_Req<1,0>           (p, id_phrase_list,      C_phrase_or_CFWS, G_Repeat<G_Req<C_Comma, C_phrase_or_CFWS>>                                       ); }
																					  																														     
		bool V_obs_utext_noCR   (ParseNode& p) { return           V_Utf8CharIf         (p,                      [] (uint c) -> bool { return c!=9 && c!=13 && c!=32; }                                             ); }
		bool V_CR_nfb_LF        (ParseNode& p) { return           V_ByteNfb            (p,                      13, 10                                                                                             ); }
		bool V_obs_utext_byte   (ParseNode& p) { return           G_Choice             (p, id_Append,           V_obs_utext_noCR, V_CR_nfb_LF                                                                      ); }
		bool C_obs_utext_word   (ParseNode& p) { return Obs(p) && G_Repeat             (p, id_utext_word,       G_Repeat<V_obs_utext_byte>                                                                         ); }
		bool C_std_utext_word   (ParseNode& p) { return           G_Req                (p, id_utext_word,       G_Repeat<V_VCHAR>                                                                                  ); }
		bool C_FWS_utext_word   (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, G_Choice<C_encw_group, C_obs_utext_word, C_std_utext_word>                                  ); }
		bool C_unstructured     (ParseNode& p) { return           G_OneOrMoreOf        (p, id_unstructured,     G_Repeat<C_FWS_utext_word>, G_Choice<C_ImfWs, C_obs_FWS>                                           ); }
																					  																														     
		bool C_std_qp           (ParseNode& p) { return           G_Req<1,1>           (p, id_quoted_pair,      V_Backslash, V_quoted_pair_char                                                                    ); }
		bool C_obs_qp           (ParseNode& p) { return Obs(p) && G_Req<1,1>           (p, id_quoted_pair,      V_Backslash, V_obs_qp_byte                                                                         ); }
		bool C_quoted_pair      (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_qp, C_obs_qp                                                                                 ); }
																					  																														     
		bool V_std_ctext_char   (ParseNode& p) { return           V_Utf8CharIf         (p,                      Is_std_ctext_char                                                                                  ); }
		bool V_obs_ctext_byte   (ParseNode& p) { return Obs(p) && V_ByteIf             (p,                      Is_obs_NO_WS_CTL_byte                                                                              ); }
		bool C_ctext_word	    (ParseNode& p) { return           G_Req                (p, id_ctext_word,       G_Repeat<G_Choice<V_std_ctext_char, V_obs_ctext_byte>>                                             ); }
		bool C_ccontent         (ParseNode& p) { return           G_Choice             (p, id_ccontent,         C_encw_group, C_ctext_word, C_quoted_pair, C_comment                                               ); }
		bool C_FWS_ccontent     (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, C_ccontent                                                                                  ); }
		bool C_comment          (ParseNode& p) { return           G_Req<1,0,0,1>       (p, id_comment,          C_OpenBr, G_Repeat<C_FWS_ccontent>, C_FWS, C_CloseBr                                               ); }
		bool C_FWS_comment      (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, C_comment                                                                                   ); }
		bool C_CFWS_inner       (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           G_Repeat<C_FWS_comment>, C_FWS                                                                     ); }
		bool C_CFWS             (ParseNode& p) { return           G_Choice             (p, id_CFWS,             C_CFWS_inner, C_FWS                                                                                ); }
																					  																														     
		bool C_CFWS_or_Comma    (ParseNode& p) { return           G_Choice             (p, id_Append,           C_CFWS, C_Comma                                                                                    ); }
		bool C_CFWS_Comma       (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_CFWS, C_Comma                                                                                    ); }
		bool C_r_CFWS_Comma     (ParseNode& p) { return           G_Repeat             (p, id_Append,           C_CFWS_Comma                                                                                       ); }
		bool C_CFWS_list        (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_r_CFWS_Comma, C_CFWS                                                                             ); }
																					  																														     
		bool V_std_qtext_char   (ParseNode& p) { return           V_Utf8CharIf         (p,                      Is_std_qtext_char                                                                                  ); }
		bool V_obs_qtext_char   (ParseNode& p) { return           V_Utf8CharIf         (p,                      [] (uint c) -> bool { return Is_std_qtext_char(c) || Is_obs_NO_WS_CTL_byte(c); }                   ); }
		bool C_std_qtext_word   (ParseNode& p) { return           G_Req                (p, id_qtext_word,       G_Repeat<V_std_qtext_char>                                                                         ); }
		bool C_obs_qtext_word   (ParseNode& p) { return Obs(p) && G_Req                (p, id_qtext_word,       G_Repeat<V_obs_qtext_char>                                                                         ); }
		bool C_qtext_word       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_qtext_word, C_obs_qtext_word                                                                 ); }
		bool C_qcontent         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_qtext_word, C_quoted_pair                                                                        ); }
		bool C_FWS_qcontent     (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, C_qcontent                                                                                  ); }
		bool C_qs_content       (ParseNode& p) { return           G_Req<1,0>           (p, id_qs_content,       G_Repeat<C_FWS_qcontent>, C_FWS                                                                    ); }
		bool C_quoted_string    (ParseNode& p) { return           G_Req<0,1,0,1,0>     (p, id_quoted_string,    C_CFWS, C_DblQuote, C_qs_content, C_DblQuote, C_CFWS                                               ); }
																					  																														     
		bool C_std_local_part   (ParseNode& p) { return           G_Choice             (p, id_local_part,       C_dot_atom, C_quoted_string                                                                        ); }
		bool C_obs_local_part   (ParseNode& p) { return Obs(p) && G_Req<1,0>           (p, id_local_part,       C_word, G_Repeat<G_Req<C_Dot, C_word>>                                                             ); }
		bool C_local_part       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_local_part, C_obs_local_part                                                                 ); }
																					  																														     
		bool C_DomLit_content   (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           G_Repeat<C_FWS_dtext>, C_FWS                                                                       ); }
		bool C_DomLit_inner     (ParseNode& p) { return           G_Req<1,0,1>         (p, id_Append,           C_SqOpenBr, C_DomLit_content, C_SqCloseBr                                                          ); }
		bool C_domain_literal   (ParseNode& p) { return           G_Req<0,1,0>         (p, id_Append,           C_CFWS, C_DomLit_inner, C_CFWS                                                                     ); }
		bool C_std_domain       (ParseNode& p) { return           G_Choice             (p, id_domain,           C_dot_atom, C_domain_literal                                                                       ); }
		bool C_obs_domain       (ParseNode& p) { return Obs(p) && G_Req<1,0>           (p, id_domain,           C_atom, G_Repeat<G_Req<C_Dot, C_atom>>                                                             ); }
		bool C_domain           (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_domain, C_obs_domain                                                                         ); }
																					  																														     
		bool C_At_domain        (ParseNode& p) { return           G_Req<1,1>           (p, id_Append,           C_At, C_domain                                                                                     ); }
		bool C_addr_spec        (ParseNode& p) { return           G_Req<1,1,1>         (p, id_addr_spec,        C_local_part, C_At, C_domain                                                                       ); }
																					  																														     
		bool C_std_angle_addr   (ParseNode& p) { return           G_Req<0,1,1,1,0>     (p, id_angle_addr,       C_CFWS, C_Less, C_addr_spec, C_Grtr, C_CFWS                                                        ); }
		bool C_obs_dl_tail_el   (ParseNode& p) { return           G_Req<1,0,0>         (p, id_Append,           C_Comma, C_CFWS, C_At_domain                                                                       ); }
		bool C_obs_dl_tail      (ParseNode& p) { return Obs(p) && G_Repeat             (p, id_Append,           C_obs_dl_tail_el                                                                                   ); }
		bool C_obs_domain_list  (ParseNode& p) { return           G_Req<0,1,0>         (p, id_obs_domain_list,  G_Repeat<C_CFWS_or_Comma>, C_At_domain, C_obs_dl_tail                                              ); }
		bool C_obs_route        (ParseNode& p) { return           G_Req<1,1>           (p, id_Append,           C_obs_domain_list, C_Colon                                                                         ); }
		bool C_obs_angle_addr   (ParseNode& p) { return Obs(p) && G_Req<0,1,1,1,1,0>   (p, id_angle_addr,       C_CFWS, C_Less, C_obs_route, C_addr_spec, C_Grtr, C_CFWS                                           ); }
		bool C_angle_addr       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_angle_addr, C_obs_angle_addr                                                                 ); }
		bool C_name_addr        (ParseNode& p) { return           G_Req<0,1>           (p, id_name_addr,        C_phrase, C_angle_addr                                                                             ); }
		bool C_mailbox          (ParseNode& p) { return           G_Choice             (p, id_mailbox,          C_name_addr, C_addr_spec                                                                           ); }
																					  																														     
		bool C_std_mbox_list    (ParseNode& p) { return           G_Req<1,0>           (p, id_mailbox_list,     C_mailbox, G_Repeat<G_Req<C_Comma, C_mailbox>>                                                     ); }
		bool C_mbox_or_CFWS     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_mailbox, C_CFWS                                                                                  ); }
		bool C_obs_ml_tail      (ParseNode& p) { return           G_Repeat             (p, id_Append,           G_Req<C_Comma, C_mbox_or_CFWS>                                                                     ); }
		bool C_obs_mbox_list    (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_mailbox_list,     C_r_CFWS_Comma, C_mailbox, C_obs_ml_tail                                                           ); }
		bool C_mailbox_list     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_mbox_list, C_obs_mbox_list                                                                   ); }
		bool C_obs_group_list   (ParseNode& p) { return Obs(p) && G_Req<1,1>           (p, id_Append,           C_r_CFWS_Comma, C_CFWS                                                                             ); }
		bool C_group_list       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_mailbox_list, C_CFWS, C_obs_group_list                                                           ); }
		bool C_group            (ParseNode& p) { return           G_Req<1,1,0,1,0>     (p, id_group,            C_phrase, C_Colon, C_group_list, C_Semicolon, C_CFWS                                               ); }
		bool C_address          (ParseNode& p) { return           G_Choice             (p, id_address,          C_mailbox, C_group                                                                                 ); }
																					  																														     
		bool C_std_addr_list    (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           C_address, G_Repeat<G_Req<C_Comma, C_address>>                                                     ); }
		bool V_casual_addr_seps (ParseNode& p) { return           V_Utf8CharIf         (p,                      [] (uint c) -> bool { return !!ZChr(";, \t\r\n", c); }                                             ); }
		bool C_casual_addr_seps (ParseNode& p) { return           G_Repeat             (p, id_casual_addr_seps, V_casual_addr_seps                                                                                 ); }
		bool C_casual_addr_elem (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_casual_addr_seps, C_address                                                                      ); }
		bool C_casual_addr_list (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           G_Repeat<C_casual_addr_elem>, C_casual_addr_seps                                                   ); }
		bool C_addr_or_CFWS     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_address, C_CFWS                                                                                  ); }
		bool C_obs_al_tail      (ParseNode& p) { return           G_Repeat             (p, id_Append,           G_Req<C_Comma, C_addr_or_CFWS>                                                                     ); }
		bool C_obs_addr_list    (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_Append,           C_r_CFWS_Comma, C_address, C_obs_al_tail                                                           ); }
		bool C_address_list     (ParseNode& p) { return           G_Choice             (p, id_address_list,     C_std_addr_list, C_obs_addr_list                                                                   ); }
																					  																														     
		bool C_addrlist_or_CFWS (ParseNode& p) { return           G_Choice             (p, id_Append,           C_address_list, C_CFWS                                                                             ); }
		bool C_addr_or_CFWSlist (ParseNode& p) { return           G_Choice             (p, id_Append,           C_address_list, C_CFWS_list                                                                        ); }
																					  																														     
		bool C_2digits          (ParseNode& p) { return           G_Req                (p, id_2digits,          V_2digits                                                                                          ); }
		bool C_4digits          (ParseNode& p) { return           G_Req                (p, id_4digits,          V_4digits                                                                                          ); }
																					  																														     
		bool C_day_name         (ParseNode& p) { return           G_Req                (p, id_day_name,         V_day_name                                                                                         ); }
		bool C_FWS_day_name     (ParseNode& p) { return           G_Req<0,1>           (p, id_Append,           C_FWS, C_day_name                                                                                  ); }
		bool C_obs_dayofweek    (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_Append,           C_CFWS, C_day_name, C_CFWS                                                                         ); }
		bool C_dayofweek        (ParseNode& p) { return           G_Choice             (p, id_Append,           C_FWS_day_name, C_obs_dayofweek                                                                    ); }
		bool C_day_digits       (ParseNode& p) { return           G_Req                (p, id_day_digits,       V_1or2digits                                                                                       ); }
		bool C_std_day          (ParseNode& p) { return           G_Req<0,1,1>         (p, id_day,              C_FWS, C_day_digits, C_FWS                                                                         ); }
		bool C_obs_day          (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_day,              C_CFWS, C_day_digits, C_CFWS                                                                       ); }
		bool C_day              (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_day, C_obs_day                                                                               ); }
		bool C_month            (ParseNode& p) { return           G_Req                (p, id_month,            V_month                                                                                            ); }
		bool C_year_digits      (ParseNode& p) { return           G_Req                (p, id_year_digits,      V_4plusdigits                                                                                      ); }
		bool C_std_year         (ParseNode& p) { return           G_Req<1,1,1>         (p, id_year,             C_FWS, C_year_digits, C_FWS                                                                        ); }
		bool C_obs_year_digits  (ParseNode& p) { return           G_Req                (p, id_year_digits,      V_2plusdigits                                                                                      ); }
		bool C_obs_year         (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_year,             C_CFWS, C_obs_year_digits, C_CFWS                                                                  ); }
		bool C_year             (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_year, C_obs_year                                                                             ); }
		bool C_date             (ParseNode& p) { return           G_Req<1,1,1>         (p, id_date,             C_day, C_month, C_year                                                                             ); }
		bool C_obs_timepart     (ParseNode& p) { return Obs(p) && G_Req<0,1,0>         (p, id_Append,           C_CFWS, C_2digits, C_CFWS                                                                          ); }
		bool C_timepart         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_2digits, C_obs_timepart                                                                          ); }
		bool C_timeofday        (ParseNode& p) { return           G_Req<1,1,1,0>       (p, id_Append,           C_timepart, C_Colon, C_timepart, G_Req<C_Colon, C_timepart>                                        ); }
		bool C_Plus_or_Dash     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_Plus, C_Dash                                                                                     ); }

		// Permit obsolete time zones even without Obs(p) - implementations use them in Delivery Status Notifications
		bool C_std_zone         (ParseNode& p) { return           G_Req<1,1>           (p, id_zone,             C_Plus_or_Dash, C_4digits                                                                          ); }
		bool C_obs_zone         (ParseNode& p) { return           G_Req                (p, id_zone,             V_obs_zone                                                                                         ); }
		bool C_zone             (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_zone, C_obs_zone                                                                             ); }
		bool C_time             (ParseNode& p) { return           G_Req<1,1,1>         (p, id_time,             C_timeofday, C_FWS, C_zone                                                                         ); }
		bool C_date_time        (ParseNode& p) { return           G_Req<0,1,1,0>       (p, id_date_time,        G_Req<C_dayofweek, C_Comma>, C_date, C_time, C_CFWS                                                ); }
																					  																														     
		bool C_obs_id_left      (ParseNode& p) { return Obs(p) && G_Req                (p, id_Append,           C_local_part                                                                                       ); }
		bool C_id_left          (ParseNode& p) { return           G_Choice             (p, id_Append,           C_dot_atom_text, C_obs_id_left                                                                     ); }
		bool C_obs_id_right     (ParseNode& p) { return Obs(p) && G_Req                (p, id_Append,           C_domain                                                                                           ); }
		bool C_id_right         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_dot_atom_text, C_no_fold_literal, C_obs_id_right                                                 ); }
		bool C_msg_id_inner     (ParseNode& p) { return           G_Req<1,1,1>         (p, id_msg_id,           C_id_left, C_At, C_id_right                                                                        ); }
		bool C_msg_id_outer     (ParseNode& p) { return           G_Req<0,1,1,1,0>     (p, id_Append,           C_CFWS, C_Less, C_msg_id_inner, C_Grtr, C_CFWS                                                     ); }
		bool C_r_msg_id_outer   (ParseNode& p) { return           G_Repeat             (p, id_Append,           C_msg_id_outer                                                                                     ); }
		bool C_phrase_or_msg_id (ParseNode& p) { return           G_Choice             (p, id_Append,           C_phrase, C_msg_id_outer                                                                           ); }
		bool C_r_phraseormsgid  (ParseNode& p) { return           G_Repeat             (p, id_Append,           C_phrase_or_msg_id                                                                                 ); }

		bool C_kw_no            (ParseNode& p) { return           G_Req                (p, id_kw_no,            V_kw_no                                                                                            ); }
		bool C_kw_auto_gen      (ParseNode& p) { return           G_Req                (p, id_kw_auto_gen,      V_kw_auto_gen                                                                                      ); }
		bool C_kw_auto_repl     (ParseNode& p) { return           G_Req                (p, id_kw_auto_repl,     V_kw_auto_repl                                                                                     ); }
																					  																														     
		bool C_kw_return        (ParseNode& p) { return           G_Req                (p, id_kw_return,        V_kw_return                                                                                        ); }
		bool C_kw_received      (ParseNode& p) { return           G_Req                (p, id_kw_received,      V_kw_received                                                                                      ); }
		bool C_kw_resent_date   (ParseNode& p) { return           G_Req                (p, id_kw_resent_date,   V_kw_resent_date                                                                                   ); }
		bool C_kw_resent_from   (ParseNode& p) { return           G_Req                (p, id_kw_resent_from,   V_kw_resent_from                                                                                   ); }
		bool C_kw_resent_sender (ParseNode& p) { return           G_Req                (p, id_kw_resent_sender, V_kw_resent_sender                                                                                 ); }
		bool C_kw_resent_to     (ParseNode& p) { return           G_Req                (p, id_kw_resent_to,     V_kw_resent_to                                                                                     ); }
		bool C_kw_resent_cc     (ParseNode& p) { return           G_Req                (p, id_kw_resent_cc,     V_kw_resent_cc                                                                                     ); }
		bool C_kw_resent_bcc    (ParseNode& p) { return           G_Req                (p, id_kw_resent_bcc,    V_kw_resent_bcc                                                                                    ); }
		bool C_kw_resent_msg_id (ParseNode& p) { return           G_Req                (p, id_kw_resent_msg_id, V_kw_resent_msg_id                                                                                 ); }
		bool C_kw_resent_rply   (ParseNode& p) { return           G_Req                (p, id_kw_resent_rply,   V_kw_resent_rply                                                                                   ); }
		bool C_kw_date          (ParseNode& p) { return           G_Req                (p, id_kw_date,          V_kw_date                                                                                          ); }
		bool C_kw_from          (ParseNode& p) { return           G_Req                (p, id_kw_from,          V_kw_from                                                                                          ); }
		bool C_kw_sender        (ParseNode& p) { return           G_Req                (p, id_kw_sender,        V_kw_sender                                                                                        ); }
		bool C_kw_reply_to      (ParseNode& p) { return           G_Req                (p, id_kw_reply_to,      V_kw_reply_to                                                                                      ); }
		bool C_kw_to            (ParseNode& p) { return           G_Req                (p, id_kw_to,            V_kw_to                                                                                            ); }
		bool C_kw_cc            (ParseNode& p) { return           G_Req                (p, id_kw_cc,            V_kw_cc                                                                                            ); }
		bool C_kw_bcc           (ParseNode& p) { return           G_Req                (p, id_kw_bcc,           V_kw_bcc                                                                                           ); }
		bool C_kw_message_id    (ParseNode& p) { return           G_Req                (p, id_kw_message_id,    V_kw_message_id                                                                                    ); }
		bool C_kw_in_reply_to   (ParseNode& p) { return           G_Req                (p, id_kw_in_reply_to,   V_kw_in_reply_to                                                                                   ); }
		bool C_kw_references    (ParseNode& p) { return           G_Req                (p, id_kw_references,    V_kw_references                                                                                    ); }
		bool C_kw_subject       (ParseNode& p) { return           G_Req                (p, id_kw_subject,       V_kw_subject                                                                                       ); }
		bool C_kw_comments      (ParseNode& p) { return           G_Req                (p, id_kw_comments,      V_kw_comments                                                                                      ); }
		bool C_kw_keywords      (ParseNode& p) { return           G_Req                (p, id_kw_keywords,      V_kw_keywords                                                                                      ); }
		bool C_kw_dkim_sig      (ParseNode& p) { return           G_Req                (p, id_kw_dkim_sig,      V_kw_dkim_sig                                                                                      ); }
		bool C_kw_auto_sbmtd    (ParseNode& p) { return           G_Req                (p, id_kw_auto_sbmtd,    V_kw_auto_sbmtd                                                                                    ); }
																					  																														     
		bool C_empty_path       (ParseNode& p) { return           G_Req<0,1,0,1,0>     (p, id_Append,           C_CFWS, C_Less, C_CFWS, C_Grtr, C_CFWS                                                             ); }
		bool C_path             (ParseNode& p) { return           G_Choice             (p, id_path,             C_angle_addr, C_empty_path                                                                         ); }
		bool C_std_return       (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_return,           C_kw_return, C_Colon, C_path, C_CRLF                                                               ); }
		bool C_obs_return       (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_return,           C_kw_return, C_ImfWs, C_Colon, C_path, C_CRLF                                                      ); }
		bool C_return           (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_return, C_obs_return                                                                         ); }
																					  																														     
		bool C_rcvd_token       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_angle_addr, C_addr_spec, C_domain, C_word                                                        ); }
		bool C_Semicol_datetime (ParseNode& p) { return           G_Req<1,1>           (p, id_Append,           C_Semicolon, C_date_time                                                                           ); }
		bool C_received_tokens  (ParseNode& p) { return           G_Choice             (p, id_Append,           G_Repeat<C_rcvd_token>, C_CFWS                                                                     ); } 
		bool C_std_received     (ParseNode& p) { return           G_Req<1,1,0,1,1>     (p, id_received,         C_kw_received, C_Colon, C_received_tokens, C_Semicol_datetime, C_CRLF                              ); }
		bool C_obs_received     (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,0,1>   (p, id_obs_received,     C_kw_received, C_ImfWs, C_Colon, C_received_tokens,						     		                  
		                                                                                                        C_Semicol_datetime, C_CRLF                                                                         ); }
		bool C_received         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_received, C_obs_received                                                                     ); }
																					  																														     
		bool C_trace_field      (ParseNode& p) { return           G_Choice             (p, id_Append,           C_return, C_received                                                                               ); }
		bool C_trace_field_grp  (ParseNode& p) { return           G_Req<0,0,1>         (p, id_trace_fields,     C_return, G_Repeat<C_trr_opt_inv_fld>, G_Repeat<C_received>                                        ); }
																					  																														     
		bool C_std_resent_date  (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_date,      C_kw_resent_date, C_Colon, C_date_time, C_CRLF                                                     ); }
		bool C_obs_resent_date  (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_date,      C_kw_resent_date, C_ImfWs, C_Colon, C_date_time, C_CRLF                                            ); }
		bool C_resent_date      (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_date, C_obs_resent_date                                                               ); }
																					  																														     
		bool C_std_resent_from  (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_from,      C_kw_resent_from, C_Colon, C_address_list, C_CRLF                                                  ); }
		bool C_obs_resent_from  (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_from,      C_kw_resent_from, C_ImfWs, C_Colon, C_address_list, C_CRLF                                         ); }
		bool C_resent_from      (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_from, C_obs_resent_from                                                               ); }
																					  																														     
		bool C_std_resent_send  (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_sender,    C_kw_resent_sender, C_Colon, C_address, C_CRLF                                                     ); }
		bool C_obs_resent_send  (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_sender,    C_kw_resent_sender, C_ImfWs, C_Colon, C_address, C_CRLF                                            ); }
		bool C_resent_sender    (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_send, C_obs_resent_send                                                               ); }
																					  																														     
		bool C_std_resent_to    (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_to,        C_kw_resent_to, C_Colon, C_address_list, C_CRLF                                                    ); }
		bool C_obs_resent_to    (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_to,        C_kw_resent_to, C_ImfWs, C_Colon, C_address_list, C_CRLF                                           ); }
		bool C_resent_to        (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_to, C_obs_resent_to                                                                   ); }
																					  																														     
		bool C_std_resent_cc    (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_cc,        C_kw_resent_cc, C_Colon, C_address_list, C_CRLF                                                    ); }
		bool C_obs_resent_cc    (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_cc,        C_kw_resent_cc, C_ImfWs, C_Colon, C_address_list, C_CRLF                                           ); }
		bool C_resent_cc        (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_cc, C_obs_resent_cc                                                                   ); }
																					  																														     
		bool C_std_resent_bcc   (ParseNode& p) { return           G_Req<1,1,0,1>       (p, id_resent_bcc,       C_kw_resent_bcc, C_Colon, C_addrlist_or_CFWS, C_CRLF                                               ); }
		bool C_obs_resent_bcc   (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_resent_bcc,       C_kw_resent_bcc, C_ImfWs, C_Colon, C_addr_or_CFWSlist, C_CRLF                                      ); }
		bool C_resent_bcc       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_bcc, C_obs_resent_bcc                                                                 ); }
																					  																														     
		bool C_std_resent_mid   (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_resent_msg_id,    C_kw_resent_msg_id, C_Colon, C_msg_id_outer, C_CRLF                                                ); }
		bool C_obs_resent_mid   (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_msg_id,    C_kw_resent_msg_id, C_ImfWs, C_Colon, C_msg_id_outer, C_CRLF                                       ); }
		bool C_resent_msg_id    (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_resent_mid, C_obs_resent_mid                                                                 ); }
																					  																														     
		bool C_obs_resent_rply  (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_resent_rply,      C_kw_resent_rply, C_ImfWs, C_Colon, C_address_list, C_CRLF                                         ); }
																					  																														     
		bool C_resent_field     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_resent_date, C_resent_from, C_resent_sender, C_resent_to, C_resent_cc,			                     
		                                                                                                        C_resent_bcc,  C_resent_msg_id, C_obs_resent_rply                                                  ); }
																					  																														     
		bool C_std_orig_date    (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_orig_date,        C_kw_date, C_Colon, C_date_time, C_CRLF                                                            ); }
		bool C_obs_orig_date    (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_orig_date,        C_kw_date, C_ImfWs, C_Colon, C_date_time, C_CRLF                                                   ); }
		bool C_orig_date        (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_orig_date, C_obs_orig_date                                                                   ); }
																					  																														     
		bool C_std_from         (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_from,             C_kw_from, C_Colon, C_address_list, C_CRLF                                                         ); }
		bool C_obs_from         (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_from,             C_kw_from, C_ImfWs, C_Colon, C_address_list, C_CRLF                                                ); }
		bool C_from             (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_from, C_obs_from                                                                             ); }
																					  																														     
		bool C_std_sender       (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_sender,           C_kw_sender, C_Colon, C_address, C_CRLF                                                            ); }
		bool C_obs_sender       (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_sender,           C_kw_sender, C_ImfWs, C_Colon, C_address, C_CRLF                                                   ); }
		bool C_sender           (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_sender, C_obs_sender                                                                         ); }
																					  																														     
		bool C_std_reply_to     (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_reply_to,         C_kw_reply_to, C_Colon, C_address_list, C_CRLF                                                     ); }
		bool C_obs_reply_to     (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_reply_to,         C_kw_reply_to, C_ImfWs, C_Colon, C_address_list, C_CRLF                                            ); }
		bool C_reply_to         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_reply_to, C_obs_reply_to                                                                     ); }
																					  																														     
		bool C_origin_field     (ParseNode& p) { return           G_Choice             (p, id_Append,           C_orig_date, C_from, C_sender, C_reply_to                                                          ); }
																					  																														     
		bool C_std_to           (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_to,               C_kw_to, C_Colon, C_address_list, C_CRLF                                                           ); }
		bool C_obs_to           (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_to,               C_kw_to, C_ImfWs, C_Colon, C_address_list, C_CRLF                                                  ); }
		bool C_to               (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_to, C_obs_to                                                                                 ); }
																					  																														     
		bool C_std_cc           (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_cc,               C_kw_cc, C_Colon, C_address_list, C_CRLF                                                           ); }
		bool C_obs_cc           (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_cc,               C_kw_cc, C_ImfWs, C_Colon, C_address_list, C_CRLF                                                  ); }
		bool C_cc               (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_cc, C_obs_cc                                                                                 ); }
																					  																														     
		bool C_std_bcc          (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_bcc,              C_kw_bcc, C_Colon, C_addrlist_or_CFWS, C_CRLF                                                      ); }
		bool C_obs_bcc          (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_bcc,              C_kw_bcc, C_ImfWs, C_Colon, C_addr_or_CFWSlist, C_CRLF                                             ); }
		bool C_bcc              (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_bcc, C_obs_bcc                                                                               ); }
																					  																														     
		bool C_dest_field       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_to, C_cc, C_bcc                                                                                  ); }
																					  																														     
		bool C_std_message_id   (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_message_id,       C_kw_message_id, C_Colon, C_msg_id_outer, C_CRLF                                                   ); }
		bool C_obs_message_id   (ParseNode& p) { return Obs(p) && G_Req<1,0,1,1,1>     (p, id_message_id,       C_kw_message_id, C_ImfWs, C_Colon, C_msg_id_outer, C_CRLF                                          ); }
		bool C_message_id       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_message_id, C_obs_message_id                                                                 ); }
																					  																														     
		bool C_std_in_reply_to  (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_in_reply_to,      C_kw_in_reply_to, C_Colon, C_r_msg_id_outer, C_CRLF                                                ); }
		bool C_obs_in_reply_to  (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_in_reply_to,      C_kw_in_reply_to, C_ImfWs, C_Colon, C_r_phraseormsgid, C_CRLF                                      ); }
		bool C_in_reply_to      (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_in_reply_to, C_obs_in_reply_to                                                               ); }
																					  																														     
		bool C_std_references   (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_references,       C_kw_references, C_Colon, C_r_msg_id_outer, C_CRLF                                                 ); }
		bool C_obs_references   (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_references,       C_kw_references, C_ImfWs, C_Colon, C_r_phraseormsgid, C_CRLF                                       ); }
		bool C_references       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_references, C_obs_references                                                                 ); }
																					  																														     
		bool C_id_field         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_message_id, C_in_reply_to, C_references                                                          ); }
																					  																														     
		bool C_std_subject      (ParseNode& p) { return           G_Req<1,1,0,1>       (p, id_subject,          C_kw_subject, C_Colon, C_unstructured, C_CRLF                                                      ); }
		bool C_obs_subject      (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_subject,          C_kw_subject, C_ImfWs, C_Colon, C_unstructured, C_CRLF                                             ); }
		bool C_subject          (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_subject, C_obs_subject                                                                       ); }
																					  																														     
		bool C_std_comments     (ParseNode& p) { return           G_Req<1,1,0,1>       (p, id_comments,         C_kw_comments, C_Colon, C_unstructured, C_CRLF                                                     ); }
		bool C_obs_comments     (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_comments,         C_kw_comments, C_ImfWs, C_Colon, C_unstructured, C_CRLF                                            ); }
		bool C_comments         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_comments, C_obs_comments                                                                     ); }
																					  																														     
		bool C_std_keywords     (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_keywords,         C_kw_keywords, C_Colon, C_std_phrase_list, C_CRLF                                                  ); }
		bool C_obs_keywords     (ParseNode& p) { return Obs(p) && G_Req<1,0,1,0,1>     (p, id_keywords,         C_kw_keywords, C_ImfWs, C_Colon, C_obs_phrase_list, C_CRLF                                         ); }
		bool C_keywords         (ParseNode& p) { return           G_Choice             (p, id_Append,           C_std_keywords, C_obs_keywords                                                                     ); }
																					  																														     
		bool C_info_field       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_subject, C_comments, C_keywords                                                                  ); }

		// For DKIM AUID, RFC 6376 specifies use of the stricter, less nonsense SMTP local-part and domain syntax (RFC 5321).
		// The IMF local-part and domain syntax (RFC 5322, as defined here) permits domain literals,
		// and also permits comments and folding whitespace between the components of local-part and domain.
		bool C_dkim_hdr_list    (ParseNode& p) { return           G_Req<1,0>           (p, id_dkim_hdr_list,    C_field_name, G_Repeat<G_Req<C_Colon, C_field_name>>                                               ); }
		bool C_dkim_auid        (ParseNode& p) { return           G_Req<1,1,1>         (p, id_dkim_auid,        Smtp::C_Local_part, C_At, Smtp::C_Domain                                                           ); }
		bool C_dkim_tag_name    (ParseNode& p) { return           G_Req                (p, id_dkim_tag_name,    V_dkim_tag_name                                                                                    ); }
		bool C_dkim_tag_value   (ParseNode& p) { return           G_Req                (p, id_dkim_tag_value,   V_dkim_tag_value                                                                                   ); }
		bool C_dkim_tag         (ParseNode& p) { return           G_Req<1,0,1,0,1>     (p, id_dkim_tag,         C_dkim_tag_name, C_FWS, C_Eq, C_FWS, C_dkim_tag_value                                              ); }
		bool C_dkim_tag_sep     (ParseNode& p) { return           G_Req<0,1,0>         (p, id_Append,           C_FWS, C_Semicolon, C_FWS                                                                          ); }
		bool C_dkim_tag_list    (ParseNode& p) { return           G_Req<0,1,0,0>       (p, id_dkim_tag_list,    C_FWS, C_dkim_tag, G_Repeat<G_Req<C_dkim_tag_sep, C_dkim_tag>>, C_dkim_tag_sep                     ); }
		bool C_dkim_signature   (ParseNode& p) { return           G_Req<1,1,1,1>       (p, id_dkim_signature,   C_kw_dkim_sig, C_Colon, C_dkim_tag_list, C_CRLF                                                    ); }

		bool C_auto_sbmtd_value (ParseNode& p) { return           G_Choice             (p, id_auto_sbmtd_value, C_kw_no, C_kw_auto_gen, C_kw_auto_repl, Mime::C_token                                              ); }
		bool C_auto_sbmtd_param (ParseNode& p) { return           G_Req<0,1,0,1>       (p, id_Append,           C_CFWS, C_Semicolon, C_CFWS, Mime::C_parameter                                                     ); }
		bool C_auto_submitted   (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           C_auto_sbmtd_value, G_Repeat<C_auto_sbmtd_param>                                                   ); }
		bool C_auto_sbmtd_field (ParseNode& p) { return           G_Req<1,1,0,1,0,1>   (p, id_auto_sbmtd_field, C_kw_auto_sbmtd, C_Colon, C_CFWS, C_auto_submitted, C_CFWS, C_CRLF                                 ); }
																					  																														     
		bool C_opt_field_name   (ParseNode& p);										  																											                     
		bool C_std_optional     (ParseNode& p) { return           G_Req<1,1,0,1>       (p, id_optional_field,   C_opt_field_name, C_Colon, C_unstructured, C_CRLF                                                  ); }
		bool C_obs_optional     (ParseNode& p) { return           G_Req<1,0,1,0,1>     (p, id_optional_field,   C_opt_field_name, C_ImfWs, C_Colon, C_unstructured, C_CRLF                                         ); }
		bool C_optional_field   (ParseNode& p) { return           G_Choice             (p, id_Append,           C_dkim_signature, C_auto_sbmtd_field, C_std_optional, C_obs_optional                               ); }

		bool C_inv_trr_fld_name (ParseNode& p);
		bool C_inv_trr_fld      (ParseNode& p) { return           G_Req<1,0,1,0,1>     (p, id_invalid_field,    C_inv_trr_fld_name, C_ImfWs, C_Colon, C_unstructured, C_CRLF                                       ); }
		bool C_trr_opt_inv_fld  (ParseNode& p) { return           G_Choice             (p, id_Append,           C_optional_field, C_inv_trr_fld                                                                    ); }																					  																														     
		bool C_opt_or_rsnt_flds (ParseNode& p) { return           G_Choice             (p, id_Append,           G_Repeat<C_trr_opt_inv_fld>, G_Repeat<C_resent_field>                                              ); }
		bool C_tr_resent_fields (ParseNode& p) { return           G_Req<1,0>           (p, id_tr_resent_fields, C_trace_field_grp, C_opt_or_rsnt_flds                                                              ); }

		bool C_inv_field_name   (ParseNode& p) { return           G_Req                (p, id_inv_field_name,   V_field_name                                                                                       ); }
		bool C_invalid_field    (ParseNode& p) { return           G_Req<1,0,1,0,1>     (p, id_invalid_field,    C_inv_field_name, C_ImfWs, C_Colon, C_unstructured, C_CRLF                                         ); }
		bool C_opt_inv_field    (ParseNode& p) { return           G_Choice             (p, id_Append,           C_optional_field, C_invalid_field                                                                  ); }																					  																														     
		bool C_main_field       (ParseNode& p) { return           G_Choice             (p, id_Append,           C_origin_field, C_dest_field, C_id_field, C_info_field, Mime::C_msg_header_field, C_opt_inv_field  ); }
		bool C_main_fields      (ParseNode& p) { return           G_Repeat             (p, id_main_fields,      C_main_field                                                                                       ); }

		bool C_any_field_noMime (ParseNode& p) { return           G_Choice             (p, id_Append,           C_trace_field, C_resent_field, C_origin_field, C_dest_field, C_id_field,
		                                                                                                        C_info_field, C_optional_field                                                                     ); }

		bool C_message_fields   (ParseNode& p) { return           G_Req<0,1>           (p, id_message_fields,   G_Repeat<C_tr_resent_fields>, C_main_fields                                                        ); }
																					  																														     
		bool C_body             (ParseNode& p) { return           G_Repeat             (p, id_body,             V_ByteAny                                                                                          ); }
		bool C_CRLF_body        (ParseNode& p) { return           G_Req<1,0>           (p, id_Append,           C_CRLF, C_body                                                                                     ); }
		bool C_message          (ParseNode& p) { return           G_Req<1,0,1>         (p, id_message,          C_message_fields, C_CRLF_body, N_End                                                               ); }


		bool C_opt_field_name(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_opt_field_name);
			if (!V_field_name(*pn))
				return p.FailChild(pn);

			// To match this rule, must NOT be a known field name

				// Known MIME fields
			if (pn->SrcText().StartsWithInsensitive("Content-"         ) ||
				pn->SrcText().EqualInsensitive     ("MIME-Version"     ) ||

				// Known IMF trace/recent fields
			    pn->SrcText().EqualInsensitive     ("Return-Path"      ) ||
				pn->SrcText().EqualInsensitive     ("Received"         ) ||
				pn->SrcText().EqualInsensitive     ("Resent-Date"      ) ||
				pn->SrcText().EqualInsensitive     ("Resent-From"      ) ||
				pn->SrcText().EqualInsensitive     ("Resent-Sender"    ) ||
				pn->SrcText().EqualInsensitive     ("Resent-To"        ) ||
				pn->SrcText().EqualInsensitive     ("Resent-Cc"        ) ||
				pn->SrcText().EqualInsensitive     ("Resent-Bcc"       ) ||
				pn->SrcText().EqualInsensitive     ("Resent-Message-ID") ||
				pn->SrcText().EqualInsensitive     ("Resent-Reply-To"  ) ||
				pn->SrcText().EqualInsensitive     ("DKIM-Signature"   ) ||

				// Known IMF main fields
				pn->SrcText().EqualInsensitive     ("Auto-Submitted"   ) ||
				pn->SrcText().EqualInsensitive     ("Date"             ) ||
				pn->SrcText().EqualInsensitive     ("From"             ) ||
				pn->SrcText().EqualInsensitive     ("Sender"           ) ||
				pn->SrcText().EqualInsensitive     ("Reply-To"         ) ||
				pn->SrcText().EqualInsensitive     ("To"               ) ||
				pn->SrcText().EqualInsensitive     ("Cc"               ) ||
				pn->SrcText().EqualInsensitive     ("Bcc"              ) ||
				pn->SrcText().EqualInsensitive     ("Message-ID"       ) ||
				pn->SrcText().EqualInsensitive     ("In-Reply-To"      ) ||
				pn->SrcText().EqualInsensitive     ("References"       ) ||
				pn->SrcText().EqualInsensitive     ("Subject"          ) ||
				pn->SrcText().EqualInsensitive     ("Comments"         ) ||
				pn->SrcText().EqualInsensitive     ("Keywords"         ))
			{
				return p.FailChild(pn);
			}

			return p.CommitChild(pn);
		}


		bool C_inv_trr_fld_name(ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_opt_field_name);
			if (!V_field_name(*pn))
				return p.FailChild(pn);

			// To match this rule, must NOT be Received, or a main group field name

				// Known MIME fields
			if (pn->SrcText().StartsWithInsensitive("Content-"         ) ||
				pn->SrcText().EqualInsensitive     ("MIME-Version"     ) ||

				// Received
				pn->SrcText().EqualInsensitive     ("Received"         ) ||

				// Known IMF main fields
				pn->SrcText().EqualInsensitive     ("Date"             ) ||
				pn->SrcText().EqualInsensitive     ("From"             ) ||
				pn->SrcText().EqualInsensitive     ("Sender"           ) ||
				pn->SrcText().EqualInsensitive     ("Reply-To"         ) ||
				pn->SrcText().EqualInsensitive     ("To"               ) ||
				pn->SrcText().EqualInsensitive     ("Cc"               ) ||
				pn->SrcText().EqualInsensitive     ("Bcc"              ) ||
				pn->SrcText().EqualInsensitive     ("Message-ID"       ) ||
				pn->SrcText().EqualInsensitive     ("In-Reply-To"      ) ||
				pn->SrcText().EqualInsensitive     ("References"       ) ||
				pn->SrcText().EqualInsensitive     ("Subject"          ) ||
				pn->SrcText().EqualInsensitive     ("Comments"         ) ||
				pn->SrcText().EqualInsensitive     ("Keywords"         ))
			{
				return p.FailChild(pn);
			}

			return p.CommitChild(pn);
		}

	}
}
