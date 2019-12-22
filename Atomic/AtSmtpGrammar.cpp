#include "AtIncludes.h"
#include "AtSmtpGrammar.h"

#include "AtImfGrammar.h"


namespace At
{
	namespace Smtp
	{

		using namespace Parse;

		DEF_RUID_B(dcontent_str)
		DEF_RUID_B(qtextSMTP)
		DEF_RUID_B(quoted_pairSMTP)
		DEF_RUID_B(QcontentSMTP)
		DEF_RUID_B(Quoted_string)
		DEF_RUID_B(Ldh_str)
	
		DEF_RUID_B(sub_domain)
		DEF_RUID_B(Domain)
		DEF_RUID_B(At_domain)
		DEF_RUID_B(Comma_At_domain)
		DEF_RUID_B(AtDomainList)
		DEF_RUID_B(Snum)
		DEF_RUID_B(Dot_Snum)
		DEF_RUID_B(AddrLitIpv4)
		DEF_RUID_B(IPv6_hex)
		DEF_RUID_B(Colon_IPv6_hex)
		DEF_RUID_B(1r3_Colon_IPv6_hex)
		DEF_RUID_B(1r5_Colon_IPv6_hex)
		DEF_RUID_B(5r5_Colon_IPv6_hex)
		DEF_RUID_B(7r7_Colon_IPv6_hex)
		DEF_RUID_B(IPv6_full)
		DEF_RUID_B(IPv6_comp_part)
		DEF_RUID_B(IPv6_comp_sep)
		DEF_RUID_B(IPv6_comp)
		DEF_RUID_B(IPv6v4_full)
		DEF_RUID_B(IPv6v4_comp_part)
		DEF_RUID_B(IPv6v4_comp)
		DEF_RUID_B(IPv6_addr)
		DEF_RUID_B(IPv6_prefix)
		DEF_RUID_B(AddrLitIpv6)
		DEF_RUID_B(AddrLitGeneral)
		DEF_RUID_B(AddrLitInner)
		DEF_RUID_B(AddrLit)
		DEF_RUID_B(Atom)
		DEF_RUID_B(Dot_string)
		DEF_RUID_B(Local_part)
		DEF_RUID_B(Domain_or_AddrLit)
		DEF_RUID_B(Mailbox)
		DEF_RUID_B(AtDomainList_Colon)
		DEF_RUID_B(Path)
		DEF_RUID_B(EmptyPath)
		DEF_RUID_B(Reverse_path)

		DEF_RUID_B(SizeParamKw)
		DEF_RUID_B(SizeParamVal)
		DEF_RUID_B(SizeParam)

		DEF_RUID_B(BodyParamKw)
		DEF_RUID_B(BodyParam7BitKw)
		DEF_RUID_B(BodyParam8BitMimeKw)
		DEF_RUID_B(BodyParam)

		DEF_RUID_B(esmtp_keyword)
		DEF_RUID_B(esmtp_value)
		DEF_RUID_B(esmtp_param)

		DEF_RUID_B(MailParams)
	
		bool V_dcontent               (ParseNode& p) { return V_ByteIf              (p,                         [] (uint c) { return (c >= 33 && c <= 90) || (c >= 94 && c <= 126); }                         ); }
		bool V_dcontent_str           (ParseNode& p) { return G_Repeat              (p, id_Append,              V_dcontent                                                                                    ); }
        bool C_dcontent_str           (ParseNode& p) { return G_Req                 (p, id_dcontent_str,        V_dcontent_str                                                                                ); }

		bool V_qtextSMTP              (ParseNode& p) { return V_ByteIf              (p,                         [] (uint c) { return c == 32 || c == 33 || (c >= 35 && c <= 91) || (c >= 93 && c <= 126); }   ); }
        bool C_qtextSMTP              (ParseNode& p) { return G_Req                 (p, id_qtextSMTP,           V_qtextSMTP                                                                                   ); }
		bool V_quoted_pairSMTP_ch     (ParseNode& p) { return V_ByteIf              (p,                         [] (uint c) { return c >= 32 && c <= 126; }                                                   ); }
        bool C_quoted_pairSMTP        (ParseNode& p) { return G_Req<1,1>            (p, id_quoted_pairSMTP,     V_Backslash, V_quoted_pairSMTP_ch                                                             ); }
        bool C_QcontentSMTP           (ParseNode& p) { return G_Choice              (p, id_QcontentSMTP,        C_qtextSMTP, C_quoted_pairSMTP                                                                ); }
        bool C_Quoted_string          (ParseNode& p) { return G_Req<1,0,1>          (p, id_Quoted_string,       C_DblQuote, G_Repeat<C_QcontentSMTP>, C_DblQuote                                              ); }

		bool V_Ldh_str                (ParseNode& p) { return G_Repeat              (p, id_Append,              G_Choice<V_AsciiAlphaNum, G_Req<V_Dash, V_AsciiAlphaNum>>                                     ); }
        bool C_Ldh_str                (ParseNode& p) { return G_Req                 (p, id_Ldh_str,             V_Ldh_str                                                                                     ); }
        bool C_sub_domain             (ParseNode& p) { return G_Req<1,0>            (p, id_sub_domain,          V_AsciiAlphaNum, V_Ldh_str                                                                    ); }

        bool C_Domain                 (ParseNode& p) { return G_Req<1,0>            (p, id_Domain,              C_sub_domain, G_Repeat<G_Req<C_Dot, C_sub_domain>>                                            ); }
        bool C_At_domain              (ParseNode& p) { return G_Req<1,1>            (p, id_At_domain,           C_At, C_Domain                                                                                ); }
        bool C_Comma_At_domain        (ParseNode& p) { return G_Req<1,1>            (p, id_Comma_At_domain,     C_Comma, C_At_domain                                                                          ); }
        bool C_AtDomainList           (ParseNode& p) { return G_Req<1,0>            (p, id_AtDomainList,        C_At_domain, G_Repeat<C_Comma_At_domain>                                                      ); }

		bool V_Snum                   (ParseNode& p) { return G_Repeat              (p, id_Append,              V_AsciiDecDigit, 1, 3                                                                         ); }
		bool V_IPv6_hex               (ParseNode& p) { return G_Repeat              (p, id_Append,              V_AsciiHexDigit, 1, 4                                                                         ); }
        bool C_Snum                   (ParseNode& p) { return G_Req                 (p, id_Snum,                V_Snum                                                                                        ); }
        bool C_Dot_Snum               (ParseNode& p) { return G_Req<1,1>            (p, id_Dot_Snum,            C_Dot, C_Snum                                                                                 ); }
        bool C_AddrLitIpv4            (ParseNode& p) { return G_Req<1,1,1,1>        (p, id_AddrLitIpv4,         C_Snum, C_Dot_Snum, C_Dot_Snum, C_Dot_Snum                                                    ); }
        bool C_IPv6_hex               (ParseNode& p) { return G_Req                 (p, id_IPv6_hex,            V_IPv6_hex                                                                                    ); }
        bool C_Colon_IPv6_hex         (ParseNode& p) { return G_Req<1,1>            (p, id_Colon_IPv6_hex,      C_Colon, C_IPv6_hex                                                                           ); }
        bool C_1r3_Colon_IPv6_hex     (ParseNode& p) { return G_Repeat              (p, id_1r3_Colon_IPv6_hex,  C_Colon_IPv6_hex, 1, 3                                                                        ); }
        bool C_1r5_Colon_IPv6_hex     (ParseNode& p) { return G_Repeat              (p, id_1r5_Colon_IPv6_hex,  C_Colon_IPv6_hex, 1, 5                                                                        ); }
        bool C_5r5_Colon_IPv6_hex     (ParseNode& p) { return G_Repeat              (p, id_5r5_Colon_IPv6_hex,  C_Colon_IPv6_hex, 5, 5                                                                        ); }
        bool C_7r7_Colon_IPv6_hex     (ParseNode& p) { return G_Repeat              (p, id_7r7_Colon_IPv6_hex,  C_Colon_IPv6_hex, 7, 7                                                                        ); }
        bool C_IPv6_full              (ParseNode& p) { return G_Req<1,1>            (p, id_IPv6_full,           C_IPv6_hex, C_7r7_Colon_IPv6_hex                                                              ); }
        bool C_IPv6_comp_part         (ParseNode& p) { return G_Req<1,0>            (p, id_IPv6_comp_part,      C_IPv6_hex, C_1r5_Colon_IPv6_hex                                                              ); }
        bool C_IPv6_comp_sep          (ParseNode& p) { return G_SeqMatchExact       (p, id_IPv6_comp_sep,       "::"                                                                                          ); }
        bool C_IPv6_comp              (ParseNode& p) { return G_Req<0,1,0>          (p, id_IPv6_comp,           C_IPv6_comp_part, C_IPv6_comp_sep, C_IPv6_comp_part                                           ); }
        bool C_IPv6v4_full            (ParseNode& p) { return G_Req<1,1,1,1>        (p, id_IPv6v4_full,         C_IPv6_hex, C_5r5_Colon_IPv6_hex, C_Colon, C_AddrLitIpv4                                      ); }
        bool C_IPv6v4_comp_part       (ParseNode& p) { return G_Req<1,0>            (p, id_IPv6v4_comp_part,    C_IPv6_hex, C_1r3_Colon_IPv6_hex                                                              ); }
        bool C_IPv6v4_comp            (ParseNode& p) { return G_Req<0,1,0,1>        (p, id_IPv6v4_comp,         C_IPv6v4_comp_part, C_IPv6_comp_sep, C_IPv6v4_comp_part, C_AddrLitIpv4                        ); }
        bool C_IPv6_addr              (ParseNode& p) { return G_Choice              (p, id_IPv6_addr,           C_IPv6_full, C_IPv6_comp, C_IPv6v4_full, C_IPv6v4_comp                                        ); }
        bool C_IPv6_prefix            (ParseNode& p) { return G_SeqMatchInsens      (p, id_IPv6_prefix,         "IPv6:"                                                                                       ); }
        bool C_AddrLitIpv6            (ParseNode& p) { return G_Req<1,1>            (p, id_AddrLitIpv6,         C_IPv6_prefix, C_IPv6_addr                                                                    ); }
        bool C_AddrLitGeneral         (ParseNode& p) { return G_Req<1,1,1>          (p, id_AddrLitGeneral,      C_Ldh_str, C_Colon, C_dcontent_str                                                            ); }
        bool C_AddrLitInner           (ParseNode& p) { return G_Choice              (p, id_AddrLitInner,        C_AddrLitIpv4, C_AddrLitIpv6, C_AddrLitGeneral                                                ); }
        bool C_AddrLit                (ParseNode& p) { return G_Req<1,1,1>          (p, id_AddrLit,             C_SqOpenBr, C_AddrLitInner, C_SqCloseBr                                                       ); }
        bool C_Atom                   (ParseNode& p) { return G_Repeat              (p, id_Atom,                Imf::C_atext_word                                                                             ); }
        bool C_Dot_string             (ParseNode& p) { return G_Req<1,0>            (p, id_Dot_string,          C_Atom, G_Repeat<G_Req<C_Dot, C_Atom>>                                                        ); }
        bool C_Local_part             (ParseNode& p) { return G_Choice              (p, id_Local_part,          C_Dot_string, C_Quoted_string                                                                 ); }
        bool C_Domain_or_AddrLit      (ParseNode& p) { return G_Choice              (p, id_Domain_or_AddrLit,   C_Domain, C_AddrLit                                                                           ); }
        bool C_Mailbox                (ParseNode& p) { return G_Req<1,1,1>          (p, id_Mailbox,             C_Local_part, C_At, C_Domain_or_AddrLit                                                       ); }
        bool C_AtDomainList_Colon     (ParseNode& p) { return G_Req<1,1>            (p, id_AtDomainList_Colon,  C_AtDomainList, C_Colon                                                                       ); }   
        bool C_Path                   (ParseNode& p) { return G_Req<1,0,1,1>        (p, id_Path,                C_Less, C_AtDomainList_Colon, C_Mailbox, C_Grtr                                               ); }
        bool C_EmptyPath              (ParseNode& p) { return G_SeqMatchExact       (p, id_EmptyPath,           "<>"                                                                                          ); }
        bool C_Reverse_path           (ParseNode& p) { return G_Choice              (p, id_Reverse_path,        C_EmptyPath, C_Path                                                                           ); }
        
        bool C_SizeParamKw            (ParseNode& p) { return G_SeqMatchInsens      (p, id_SizeParamKw,         "size="                                                                                       ); }
        bool C_SizeParamVal           (ParseNode& p) { return G_Repeat              (p, id_SizeParamVal,        V_AsciiDecDigit                                                                               ); }
        bool C_SizeParam              (ParseNode& p) { return G_Req<1,1>            (p, id_SizeParam,           C_SizeParamKw, C_SizeParamVal                                                                 ); }

        bool C_BodyParamKw            (ParseNode& p) { return G_SeqMatchInsens      (p, id_BodyParamKw,         "body="                                                                                       ); }
        bool C_BodyParam7BitKw        (ParseNode& p) { return G_SeqMatchInsens      (p, id_BodyParam7BitKw,     "7bit"                                                                                        ); }
        bool C_BodyParam8BitMimeKw    (ParseNode& p) { return G_SeqMatchInsens      (p, id_BodyParam8BitMimeKw, "8bitmime"                                                                                    ); }
        bool C_BodyParamVal           (ParseNode& p) { return G_Choice              (p, id_Append,              C_BodyParam7BitKw, C_BodyParam8BitMimeKw                                                      ); }
        bool C_BodyParam              (ParseNode& p) { return G_Req<1,1>            (p, id_BodyParam,           C_BodyParamKw, C_BodyParamVal                                                                 ); }

        bool V_esmtp_keyword_char     (ParseNode& p) { return V_ByteIf              (p,                         [] (uint c) -> bool { return Ascii::IsAlphaNum(c) || c == '-'; }                              ); }
        bool V_r_esmtp_keyword_char   (ParseNode& p) { return G_Repeat              (p, id_Append,              V_esmtp_keyword_char                                                                          ); }
        bool C_esmtp_keyword          (ParseNode& p) { return G_Req<1,0>            (p, id_esmtp_keyword,       V_AsciiAlphaNum, V_r_esmtp_keyword_char                                                       ); }
        bool V_esmtp_value_char       (ParseNode& p) { return V_ByteIf              (p,                         [] (uint c) -> bool { return c >= 33 && c <= 126 && c != 61; }                                ); }
        bool C_esmtp_value            (ParseNode& p) { return G_Repeat              (p, id_esmtp_value,         V_esmtp_value_char                                                                            ); }
        bool C_Eq_esmtp_value         (ParseNode& p) { return G_Req<1,1>            (p, id_Append,              C_Eq, C_esmtp_value                                                                           ); }
        bool C_esmtp_param            (ParseNode& p) { return G_Req<1,1>            (p, id_esmtp_param,         C_esmtp_keyword, C_Eq_esmtp_value                                                             ); }

        bool C_MailParam              (ParseNode& p) { return G_Choice              (p, id_Append,              C_SizeParam, C_BodyParam, C_esmtp_param                                                       ); }
        bool C_Ws_MailParam           (ParseNode& p) { return G_Req<1,1>            (p, id_Append,              C_Ws, C_MailParam                                                                             ); }
        bool C_MailParams             (ParseNode& p) { return G_Req<1,0>            (p, id_MailParams,          C_Reverse_path, G_Repeat<C_Ws_MailParam>                                                      ); }

	}
}
