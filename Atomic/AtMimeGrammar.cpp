#include "AtIncludes.h"
#include "AtMimeGrammar.h"

#include "AtImfGrammar.h"

namespace At
{
	namespace Mime
	{
		using namespace Parse;

		// This grammar implements:
		// RFC 2045 - Multipurpose Internet Mail Extensions (MIME) Part One: Format of Internet Message Bodies
		// RFC 2046 - Multipurpose Internet Mail Extensions (MIME) Part Two: Media Types
		// RFC 2183 - Communicating Presentation Information in Internet Messages: The Content-Disposition Header Field
		//
		// Extensions:
		// - "@" is permitted as a boundary character: RFC 2046 does not permit it, but some real-world implementations generate it

		// Make sure to exclude the names of any new Content- fields with explicit parse rules from the generic C_extension_field_name
		bool V_kw_content_type      (ParseNode& p) { return V_SeqMatchInsens (p, "Content-Type"              ); }
		bool V_kw_content_enc       (ParseNode& p) { return V_SeqMatchInsens (p, "Content-Transfer-Encoding" ); }
		bool V_kw_content_id        (ParseNode& p) { return V_SeqMatchInsens (p, "Content-ID"                ); }
		bool V_kw_content_desc      (ParseNode& p) { return V_SeqMatchInsens (p, "Content-Description"       ); }
		bool V_kw_content_disp      (ParseNode& p) { return V_SeqMatchInsens (p, "Content-Disposition"       ); }
		bool V_kw_mime_version      (ParseNode& p) { return V_SeqMatchInsens (p, "MIME-Version"              ); }

		DEF_RUID_X(kw_content_type,      Imf::id_field_name)
		DEF_RUID_X(kw_content_enc,       Imf::id_field_name)
		DEF_RUID_X(kw_content_id,        Imf::id_field_name)
		DEF_RUID_X(kw_content_desc,      Imf::id_field_name)
		DEF_RUID_X(kw_content_disp,      Imf::id_field_name)
		DEF_RUID_X(kw_mime_version,      Imf::id_field_name)
		DEF_RUID_X(extension_field_name, Imf::id_field_name)

		DEF_RUID_B(token_text)
		DEF_RUID_B(param_name)
		DEF_RUID_B(param_value)
		DEF_RUID_B(parameter)
		DEF_RUID_B(ct_type)
		DEF_RUID_B(ct_subtype)
		DEF_RUID_B(content_type)
		DEF_RUID_B(content_enc)
		DEF_RUID_B(content_id)
		DEF_RUID_B(content_desc)
		DEF_RUID_B(disposition_type)
		DEF_RUID_B(content_disp)
		DEF_RUID_B(number_text)
		DEF_RUID_B(version_major)
		DEF_RUID_B(version_minor)
		DEF_RUID_B(mime_version)
		DEF_RUID_B(extension_field)
		DEF_RUID_B(part_header)

		DEF_RUID_B(dash_boundary)
		DEF_RUID_B(close_delimiter)
		DEF_RUID_B(prologue)
		DEF_RUID_B(epilogue)
		DEF_RUID_B(transport_padding)
		DEF_RUID_B(part_content)
		DEF_RUID_B(body_part)
		DEF_RUID_B(multipart_body)

		bool Is_token_char(uint c) { return c==33 || (c>=35 && c<=39) || c==42 || c==43 || c==45 || c==46 || (c>=48 && c<=57) || (c>=65 && c<=90) || (c>=94 && c<=126 ); }
		
		bool V_token_char           (ParseNode& p) { return V_ByteIf               (p,                       Is_token_char                                                                          ); }
		bool C_token_text           (ParseNode& p) { return G_Req                  (p, id_token_text,        G_Repeat<V_token_char>                                                                 ); }
		bool C_token                (ParseNode& p) { return G_Req<0,1,0>           (p, id_Append,            Imf::C_CFWS, C_token_text, Imf::C_CFWS                                                 ); }
		bool C_param_name           (ParseNode& p) { return G_Req                  (p, id_param_name,        C_token                                                                                ); }
		bool C_param_value          (ParseNode& p) { return G_Choice               (p, id_param_value,       C_token, Imf::C_quoted_string                                                          ); }
		bool C_parameter            (ParseNode& p) { return G_Req<1,1,1>           (p, id_parameter,         C_param_name, C_Eq, C_param_value                                                      ); }

		bool C_kw_content_type      (ParseNode& p) { return G_Req                  (p, id_kw_content_type,   V_kw_content_type                                                                      ); }
		bool C_Semicolon_parameter  (ParseNode& p) { return G_Req<1,1>             (p, id_Append,            C_Semicolon, C_parameter                                                               ); }
		bool C_ct_type              (ParseNode& p) { return G_Req                  (p, id_ct_type,           C_token                                                                                ); }
		bool C_ct_subtype           (ParseNode& p) { return G_Req                  (p, id_ct_subtype,        C_token                                                                                ); }
		bool C_content_type_inner   (ParseNode& p) { return G_Req<1,1,1,0>         (p, id_Append,            C_ct_type, C_Slash, C_ct_subtype, G_Repeat<C_Semicolon_parameter>                      ); }
		bool C_content_type         (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_content_type,      C_kw_content_type, Imf::C_ImfWs, C_Colon, C_content_type_inner, C_CRLF                 ); }

		bool C_kw_content_enc       (ParseNode& p) { return G_Req                  (p, id_kw_content_enc,    V_kw_content_enc                                                                       ); }
		bool C_content_enc          (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_content_enc,       C_kw_content_enc, Imf::C_ImfWs, C_Colon, C_token, C_CRLF                               ); }

		bool C_kw_content_id        (ParseNode& p) { return G_Req                  (p, id_kw_content_id,     V_kw_content_id                                                                        ); }
		bool C_content_id           (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_content_id,        C_kw_content_id, Imf::C_ImfWs, C_Colon, Imf::C_msg_id_outer, C_CRLF                    ); }

		bool C_kw_content_desc      (ParseNode& p) { return G_Req                  (p, id_kw_content_desc,   V_kw_content_desc                                                                      ); }
		bool C_content_desc         (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_content_desc,      C_kw_content_desc, Imf::C_ImfWs, C_Colon, Imf::C_unstructured, C_CRLF                  ); }

		bool C_kw_content_disp      (ParseNode& p) { return G_Req                  (p, id_kw_content_disp,   V_kw_content_disp                                                                      ); }
		bool C_disposition_type     (ParseNode& p) { return G_Req                  (p, id_disposition_type,  C_token                                                                                ); }
		bool C_Semicol_dispparam    (ParseNode& p) { return G_Req<1,0,1,0>         (p, id_Append,            C_Semicolon, Imf::C_CFWS, C_parameter, Imf::C_CFWS                                     ); }
		bool C_content_disp_inner   (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            C_disposition_type, G_Repeat<C_Semicol_dispparam>                                      ); }
		bool C_content_disp         (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_content_disp,      C_kw_content_disp, Imf::C_ImfWs, C_Colon, C_content_disp_inner, C_CRLF                 ); }

		bool C_kw_mime_version      (ParseNode& p) { return G_Req                  (p, id_kw_mime_version,   V_kw_mime_version                                                                      ); }
		bool V_number_char          (ParseNode& p) { return V_ByteIf               (p,                       Ascii::IsDecDigit                                                                      ); }
		bool C_number_text          (ParseNode& p) { return G_Req                  (p, id_number_text,       G_Repeat<V_number_char>                                                                ); }
		bool C_number               (ParseNode& p) { return G_Req<0,1,0>           (p, id_Append,            Imf::C_CFWS, C_number_text, Imf::C_CFWS                                                ); }
		bool C_version_major        (ParseNode& p) { return G_Req                  (p, id_version_major,     C_number                                                                               ); }
		bool C_version_minor        (ParseNode& p) { return G_Req                  (p, id_version_minor,     C_number                                                                               ); }
		bool C_major_Dot_minor      (ParseNode& p) { return G_Req<1,1,1>           (p, id_Append,            C_version_major, C_Dot, C_version_minor                                                ); }
		bool C_mime_version         (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_mime_version,      C_kw_mime_version, Imf::C_ImfWs, C_Colon, C_major_Dot_minor, C_CRLF                    ); }

		bool C_extension_field_name (ParseNode& p);
		bool C_extension_field      (ParseNode& p) { return G_Req<1,0,1,1,1>       (p, id_extension_field,   C_extension_field_name, Imf::C_ImfWs, C_Colon, Imf::C_unstructured, C_CRLF             ); }

		bool C_entity_header_field  (ParseNode& p) { return G_Choice               (p, id_Append,            C_content_type, C_content_enc, C_content_id, C_content_desc, C_content_disp,
		                                                                                                     C_extension_field                                                                      ); }
		bool C_msg_header_field     (ParseNode& p) { return G_Choice               (p, id_Append,            C_entity_header_field, C_mime_version                                                  ); }
		bool C_part_header_field    (ParseNode& p) { return G_Choice               (p, id_Append,            C_entity_header_field, Imf::C_any_field_noMime, Imf::C_invalid_field                   ); }
		bool C_part_header          (ParseNode& p) { return G_Repeat               (p, id_part_header,       C_part_header_field                                                                    ); }

		bool V_twodashes            (ParseNode& p) { return V_SeqMatchExact        (p,                       "--"                                                                                   ); }
		bool V_lone_dash            (ParseNode& p) { return V_ByteNfb              (p,                       '-', '-'                                                                               ); }
		bool V_bcharnospace         (ParseNode& p) { return V_ByteIf               (p,                    [] (uint c) -> bool { return Ascii::IsAlphaNum(c) || ZChr("'()+_,-./:=?", c) || c=='@'; } ); }
		bool V_boundary             (ParseNode& p) { return G_Repeat               (p, id_Append,            G_Choice<V_bcharnospace, G_Req<G_Repeat<V_Space>, V_bcharnospace>>                     ); }
		bool V_first_dash_boundary  (ParseNode& p) { return G_Req<1,1>             (p, id_Append,            V_twodashes, V_boundary                                                                ); }
		bool V_delim_dash_boundary  (ParseNode& p);
		bool V_close_dash_boundary  (ParseNode& p) { return G_Req<1,1>             (p, id_Append,            V_delim_dash_boundary, V_twodashes                                                     ); }

		bool V_OCTET_noCR           (ParseNode& p) { return V_ByteIf               (p,                       [] (uint c) -> bool { return c!=13; }                                                  ); }
		bool V_OCTET_noCR_nodash    (ParseNode& p) { return V_ByteIf               (p,                       [] (uint c) -> bool { return c!=13 && c!='-'; }                                        ); }
		bool V_prologuechar         (ParseNode& p) { return G_Choice               (p, id_Append,            V_OCTET_noCR, Imf::V_CR_nfb_LF                                                         ); }
		bool V_prologue_line_first  (ParseNode& p) { return G_Choice               (p, id_Append,            V_OCTET_noCR_nodash, V_lone_dash, Imf::V_CR_nfb_LF                                     ); }
		bool V_prologue_line_chars  (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            V_prologue_line_first, G_Repeat<V_prologuechar>                                        ); }
		bool V_prologue_line        (ParseNode& p) { return G_Req<0,1>             (p, id_Append,            V_prologue_line_chars, V_CRLF                                                          ); }
		bool V_epilogue             (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            V_CRLF, G_Repeat<V_ByteAny>                                                            ); }

		bool C_first_dash_boundary  (ParseNode& p) { return G_Req                  (p, id_dash_boundary,     V_first_dash_boundary                                                                  ); }
		bool C_delim_dash_boundary  (ParseNode& p) { return G_Req                  (p, id_dash_boundary,     V_delim_dash_boundary                                                                  ); }
		bool C_close_dash_boundary  (ParseNode& p) { return G_Req                  (p, id_dash_boundary,     V_close_dash_boundary                                                                  ); }
		bool C_delimiter            (ParseNode& p) { return G_Req<1,1>             (p, id_Append,            C_CRLF, C_delim_dash_boundary                                                          ); }
		bool C_close_delimiter      (ParseNode& p) { return G_Req<1,1>             (p, id_close_delimiter,   C_CRLF, C_close_dash_boundary                                                          ); }
		bool C_prologue             (ParseNode& p) { return G_Req                  (p, id_prologue,          G_Repeat<V_prologue_line>                                                              ); }
		bool C_epilogue             (ParseNode& p) { return G_Req                  (p, id_epilogue,          V_epilogue                                                                             ); }
		bool C_transport_padding    (ParseNode& p) { return G_Req                  (p, id_transport_padding, Imf::C_ImfWs                                                                           ); }
		bool C_1st_dash_boundary_tp (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            C_first_dash_boundary, C_transport_padding                                             ); }
		bool C_close_delimiter_tp   (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            C_close_delimiter, C_transport_padding                                                 ); }

		bool C_part_content         (ParseNode& p);
		bool C_CRLF_partcontent     (ParseNode& p) { return G_Req<1,0>             (p, id_Append,            C_CRLF, C_part_content                                                                 ); }
		bool C_body_part            (ParseNode& p) { return G_OneOrMoreOf          (p, id_body_part,         C_part_header, C_CRLF_partcontent                                                      ); }
		bool C_encapsulation        (ParseNode& p) { return G_Req<1,0,1,1>         (p, id_Append,            C_delimiter, C_transport_padding, C_CRLF, C_body_part                                  ); }
		bool C_multipart_body_parts (ParseNode& p) { return G_Req<1,0,1>           (p, id_Append,            C_body_part, G_Repeat<C_encapsulation>, C_close_delimiter_tp                           ); }
		bool C_multipart_body       (ParseNode& p) { return G_Req<0,1,1,1,0>       (p, id_multipart_body,    C_prologue, C_1st_dash_boundary_tp, C_CRLF, C_multipart_body_parts, C_epilogue         ); }


		bool C_extension_field_name (ParseNode& p)
		{
			ParseNode* pn = p.NewChild(id_extension_field_name);
			if (V_SeqMatchExact(*pn, "--"))
				return p.FailChild(pn);
			if (!Imf::V_field_name(*pn))
				return p.FailChild(pn);

			if (!pn->SrcText().StartsWithInsensitive("Content-"                 ) ||
				 pn->SrcText().EqualInsensitive     ("Content-Type"             ) ||
				 pn->SrcText().EqualInsensitive     ("Content-Transfer-Encoding") ||
				 pn->SrcText().EqualInsensitive     ("Content-ID"               ) ||
				 pn->SrcText().EqualInsensitive     ("Content-Description"      ) ||
				 pn->SrcText().EqualInsensitive     ("Content-Disposition"      ))
			{
				return p.FailChild(pn);
			}

			return p.CommitChild(pn);
		}


		bool V_delim_dash_boundary(ParseNode& p)
		{
			ParseNode const& bodyNode         { p.FindAncestorRef(id_multipart_body) };
			ParseNode const& dashBoundaryNode { bodyNode.FlatFindRef(id_dash_boundary) };
			Seq              dashBoundary     { dashBoundaryNode.SrcText() }; 

			return V_SeqMatchExact(p, dashBoundary);
		}


		bool C_part_content(ParseNode& p)
		{
			// Locate id_multipart_body parent, and then id_dash_boundary inside it
			ParseNode const& bodyNode         { p.FindAncestorRef(id_multipart_body) };
			ParseNode const& dashBoundaryNode { bodyNode.FlatFindRef(id_dash_boundary) };
			Seq              dashBoundary     { dashBoundaryNode.SrcText() }; 
			ParseNode*       pn               { p.NewChild(id_part_content) };

			while (true)
			{
				Seq r { pn->Remaining() };
				if (!r.n)
				{
					p.FailChild(pn);
					return false;
				}

				// Stop at CRLF dash-boundary
				if (r.p[0] == 13 &&
				    r.n >= 2 + dashBoundary.n &&
					r.p[1] == 10 &&
					r.DropBytes(2).StartsWithExact(dashBoundary))
				{
					p.CommitChild(pn);
					return true;
				}

				pn->ConsumeByte();
			}
		}
	}
}
