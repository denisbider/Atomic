#include "AtIncludes.h"
#include "AtUriGrammar.h"

#include "AtHtmlGrammar.h"

namespace At
{
	namespace Uri
	{
		using namespace Parse;

		bool Is_mark               (uint c) { return c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')'; }
		bool Is_reserved           (uint c) { return c == ';' || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' || c == '$' || c == ','; }
		bool Is_unreserved         (uint c) { return Ascii::IsAlphaNum(c) || Is_mark(c); }
		bool Is_pchar_unesc        (uint c) { return Is_unreserved(c) || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' || c == '$' || c == ','; }
		bool Is_uric_unesc         (uint c) { return Is_reserved(c) || Is_unreserved(c); }
		bool Is_uric_ns_unesc      (uint c) { return c != '/' && Is_uric_unesc(c); }
		bool Is_scheme_char        (uint c) { return Ascii::IsAlphaNum(c) || c == '+' || c == '-' || c == '.'; }
		bool Is_userinfoc_unesc    (uint c) { return Is_unreserved(c) || c == ';' || c == ':' || c == '&' || c == '=' || c == '+' || c == '$' || c == ','; }
		bool Is_regnamec_unesc     (uint c) { return Is_unreserved(c) || c == '$' || c == ',' || c == ';' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+'; }
		bool Is_relsegc_unesc      (uint c) { return Is_unreserved(c) || c == ';' || c == '@' || c == '&' || c == '=' || c == '+' || c == '$' || c == ','; }

		bool V_pchar_unesc         (ParseNode& p) { return V_ByteIf             (p, Is_pchar_unesc                                                    ); }
		bool V_hex                 (ParseNode& p) { return V_ByteIf             (p, Ascii::IsHexDigit                                                 ); }
		bool V_uric_unesc          (ParseNode& p) { return V_ByteIf             (p, Is_uric_unesc                                                     ); }
		bool V_uric_ns_unesc       (ParseNode& p) { return V_ByteIf             (p, Is_uric_ns_unesc                                                  ); }
		bool V_scheme_char         (ParseNode& p) { return V_ByteIf             (p, Is_scheme_char                                                    ); }
		bool V_userinfoc_unesc     (ParseNode& p) { return V_ByteIf             (p, Is_userinfoc_unesc                                                ); }
		bool V_regnamec_unesc      (ParseNode& p) { return V_ByteIf             (p, Is_regnamec_unesc                                                 ); }
		bool V_relsegc_unesc       (ParseNode& p) { return V_ByteIf             (p, Is_relsegc_unesc                                                  ); }
																				       
		bool V_r_Dash              (ParseNode& p) { return G_Repeat             (p, id_Append, V_Dash                                                 ); }
		bool V_Dashes_alphanum     (ParseNode& p) { return G_Req<1,1>           (p, id_Append, V_r_Dash, V_AsciiAlphaNum                              ); }
		bool V_aln_or_Dash_aln     (ParseNode& p) { return G_Choice             (p, id_Append, V_AsciiAlphaNum, V_Dashes_alphanum                     ); }
		bool V_r_aln_or_Dash_aln   (ParseNode& p) { return G_Repeat             (p, id_Append, V_aln_or_Dash_aln                                      ); }
		bool V_domainlabel         (ParseNode& p) { return G_Req<1,0>           (p, id_Append, V_AsciiAlphaNum, V_r_aln_or_Dash_aln                   ); }
		bool V_toplabel            (ParseNode& p) { return G_Req<1,0>           (p, id_Append, V_AsciiAlpha, V_r_aln_or_Dash_aln                      ); }
		bool V_domainlabel_Dot     (ParseNode& p) { return G_Req<1,1>           (p, id_Append, V_domainlabel, V_Dot                                   ); }
		bool V_r_domainlabel_Dot   (ParseNode& p) { return G_Repeat             (p, id_Append, V_domainlabel_Dot                                      ); }
		bool V_hostname            (ParseNode& p) { return G_Req<0,1,0>         (p, id_Append, V_r_domainlabel_Dot, V_toplabel, V_Dot                 ); }
		bool V_digits              (ParseNode& p) { return G_Repeat             (p, id_Append, V_AsciiDecDigit                                        ); }
		bool V_digits_Dot          (ParseNode& p) { return G_Req<1,1>           (p, id_Append, V_digits, V_Dot                                        ); }
		bool V_IPv4address         (ParseNode& p) { return G_Req<1,1,1,1>       (p, id_Append, V_digits_Dot, V_digits_Dot, V_digits_Dot, V_digits     ); }
																				       
		bool V_escaped             (ParseNode& p) { return G_Req<1,1,1>         (p, id_Append, V_Percent, V_hex, V_hex                                ); }
		bool V_pchar               (ParseNode& p) { return G_Choice             (p, id_Append, V_pchar_unesc, V_escaped                               ); }
		bool V_uric                (ParseNode& p) { return G_Choice             (p, id_Append, V_uric_unesc, V_escaped                                ); }
		bool V_uric_no_slash       (ParseNode& p) { return G_Choice             (p, id_Append, V_uric_ns_unesc, V_escaped                             ); }
		bool V_segment_char        (ParseNode& p) { return G_Choice             (p, id_Append, V_pchar, V_Semicolon                                   ); }
		bool V_userinfo_char       (ParseNode& p) { return G_Choice             (p, id_Append, V_userinfoc_unesc, V_escaped                           ); }
		bool V_regname_char        (ParseNode& p) { return G_Choice             (p, id_Append, V_regnamec_unesc, V_escaped                            ); }
		bool V_relseg_char         (ParseNode& p) { return G_Choice             (p, id_Append, V_relsegc_unesc, V_escaped                             ); }
																				       
		bool V_segment             (ParseNode& p) { return G_Repeat             (p, id_Append, V_segment_char                                         ); }
		bool V_r_uric              (ParseNode& p) { return G_Repeat             (p, id_Append, V_uric                                                 ); }
		bool V_r_scheme_char       (ParseNode& p) { return G_Repeat             (p, id_Append, V_scheme_char                                          ); }
		bool V_userinfo            (ParseNode& p) { return G_Repeat             (p, id_Append, V_userinfo_char                                        ); }
		bool V_r_digit             (ParseNode& p) { return G_Repeat             (p, id_Append, V_AsciiDecDigit                                        ); }
		bool V_reg_name            (ParseNode& p) { return G_Repeat             (p, id_Append, V_regname_char                                         ); }
		bool V_rel_segment         (ParseNode& p) { return G_Repeat             (p, id_Append, V_relseg_char                                          ); }

		bool V_SlashSlash          (ParseNode& p) { return V_SeqMatchExact      (p, "//"); }

		DEF_RUID_B(scheme)
		DEF_RUID_B(SlashSlash)
		DEF_RUID_B(domainlabel)
		DEF_RUID_B(toplabel)
		DEF_RUID_B(segment)
		DEF_RUID_B(abs_path)
		DEF_RUID_B(query)
		DEF_RUID_B(QM_query)
		DEF_RUID_B(abspath_AndOr_query)
		DEF_RUID_B(rel_segment)
		DEF_RUID_B(rel_path)

		DEF_RUID_B(userinfo)
		DEF_RUID_B(hostname)
		DEF_RUID_B(IPv4address)
		DEF_RUID_B(port)
		DEF_RUID_B(authority)
		DEF_RUID_X(server,   id_authority)
		DEF_RUID_X(reg_name, id_authority)

		DEF_RUID_B(net_path)
		DEF_RUID_B(hier_part)
		DEF_RUID_B(opaque_part)

		DEF_RUID_B(fragment)
		DEF_RUID_B(absoluteURI)
		DEF_RUID_B(relativeURI)
		DEF_RUID_B(absURI_optFragment)
		DEF_RUID_B(URI_reference)

		bool C_scheme              (ParseNode& p) { return G_Req<1,0>     (p, id_scheme,              V_AsciiAlpha, V_r_scheme_char                                     ); }
		bool C_SlashSlash          (ParseNode& p) { return G_Req          (p, id_SlashSlash,          V_SlashSlash                                                      ); }

		bool C_domainlabel         (ParseNode& p) { return G_Req          (p, id_domainlabel,         V_domainlabel                                                     ); }
		bool C_toplabel            (ParseNode& p) { return G_Req          (p, id_toplabel,            V_toplabel                                                        ); }
		bool C_segment             (ParseNode& p) { return G_Req          (p, id_segment,             V_segment                                                         ); }
		bool C_Slash_segment       (ParseNode& p) { return G_Req<1,0>     (p, id_Append,              C_Slash, C_segment                                                ); }
		bool C_path_segments       (ParseNode& p) { return G_Req<1,0>     (p, id_Append,              C_segment, G_Repeat<C_Slash_segment>                              ); }
		bool C_abs_path            (ParseNode& p) { return G_Req<1,0>     (p, id_abs_path,            C_Slash, C_path_segments                                          ); }
		bool C_query               (ParseNode& p) { return G_Req          (p, id_query,               V_r_uric                                                          ); }
		bool C_QM_query            (ParseNode& p) { return G_Req<1,0>     (p, id_QM_query,            C_QuestnMark, C_query                                             ); }
		bool C_abspath_AndOr_query (ParseNode& p) { return G_OneOrMoreOf  (p, id_abspath_AndOr_query, C_abs_path, C_QM_query                                            ); }
		bool C_rel_segment         (ParseNode& p) { return G_Req          (p, id_rel_segment,         V_rel_segment                                                     ); }
		bool C_rel_path            (ParseNode& p) { return G_Req<1,0>     (p, id_rel_path,            C_rel_segment, C_abs_path                                         ); }

		bool C_userinfo            (ParseNode& p) { return G_Req          (p, id_userinfo,            V_userinfo                                                        ); }
		bool C_userinfo_At         (ParseNode& p) { return G_Req<1,1>     (p, id_Append,              C_userinfo, C_At                                                  ); }
		bool C_hostname            (ParseNode& p) { return G_Req          (p, id_hostname,            V_hostname                                                        ); }
		bool C_IPv4address         (ParseNode& p) { return G_Req          (p, id_IPv4address,         V_IPv4address                                                     ); }
		bool C_host                (ParseNode& p) { return G_Choice       (p, id_Append,              C_hostname, C_IPv4address                                         ); }
		bool C_port                (ParseNode& p) { return G_Req          (p, id_port,                V_r_digit                                                         ); }
		bool C_Colon_port          (ParseNode& p) { return G_Req<1,1>     (p, id_Append,              C_Colon, C_port                                                   ); }
		bool C_hostport            (ParseNode& p) { return G_Req<1,0>     (p, id_Append,              C_host, C_Colon_port                                              ); }
		bool C_server              (ParseNode& p) { return G_Req<0,1>     (p, id_server,              C_userinfo_At, C_hostport                                         ); }
		bool C_reg_name            (ParseNode& p) { return G_Req          (p, id_reg_name,            V_reg_name                                                        ); }

		bool C_authority           (ParseNode& p) { return G_Choice       (p, id_Append,	          C_server, C_reg_name                                              ); }
		bool C_net_path            (ParseNode& p) { return G_Req<1,0,0>   (p, id_net_path,            C_SlashSlash, C_authority, C_abs_path                             ); }
		bool C_net_or_abs_path     (ParseNode& p) { return G_Choice       (p, id_Append,              C_net_path, C_abs_path                                            ); }
		bool C_net_abs_or_rel_path (ParseNode& p) { return G_Choice       (p, id_Append,              C_net_path, C_abs_path, C_rel_path                                ); }
		bool C_hier_part           (ParseNode& p) { return G_Req<1,0>     (p, id_hier_part,           C_net_or_abs_path, C_QM_query                                     ); }
		bool C_opaque_part         (ParseNode& p) { return G_Req<1,0>     (p, id_opaque_part,         V_uric_no_slash, V_r_uric                                         ); }
		bool C_hier_or_opaque_part (ParseNode& p) { return G_Choice       (p, id_Append,              C_hier_part, C_opaque_part                                        ); }

		bool C_fragment            (ParseNode& p) { return G_Req          (p, id_fragment,            V_r_uric                                                          ); }
		bool C_Hash_fragment       (ParseNode& p) { return G_Req<1,0>     (p, id_Append,              C_Hash, C_fragment                                                ); }
		bool C_absoluteURI         (ParseNode& p) { return G_Req<1,1,1>   (p, id_absoluteURI,         C_scheme, C_Colon, C_hier_or_opaque_part                          ); }
		bool C_relativeURI         (ParseNode& p) { return G_Req<1,0>     (p, id_relativeURI,         C_net_abs_or_rel_path, C_QM_query                                 ); }
		bool C_abs_or_rel_uri      (ParseNode& p) { return G_Choice       (p, id_Append,              C_absoluteURI, C_relativeURI                                      ); }
		bool C_absURI_optFragment  (ParseNode& p) { return G_Req<1,0>     (p, id_absURI_optFragment,  C_absoluteURI, C_Hash_fragment                                    ); }
		bool C_URI_reference       (ParseNode& p) { return G_OneOrMoreOf  (p, id_URI_reference,       C_abs_or_rel_uri, C_Hash_fragment                                 ); } 
		bool C_URI_references      (ParseNode& p) { return G_Req<1,0>     (p, id_Append,              C_URI_reference, G_Repeat<G_Req<Html::C_HtmlWs, C_URI_reference>> ); }

		bool IsValidAbsoluteUri(Seq uri)
		{
			return ParseTree(uri).Parse(C_absoluteURI);
		}

		bool IsValidRelativeUri(Seq uri)
		{
			return ParseTree(uri).Parse(C_relativeURI);
		}

		bool IsValidUri(Seq uri)
		{
			return ParseTree(uri).Parse(C_URI_reference);
		}


		Seq FindLeadingUri(Seq s, ParseTree::Storage* storage)
		{
			auto parseFunc = [] (ParseNode& p) -> bool
				{ return G_Req<1,0>(p, id_Append, C_absURI_optFragment, C_Remaining); };

			ParseTree pt { s, storage };
			if (!pt.Parse(parseFunc))
				return Seq();

			return pt.Root().FrontFindRef(id_absURI_optFragment).SrcText();
		}


		void FindUrisInText(Seq text, Vec<Seq>& uris, ParseTree::Storage* storage)
		{
			ParseTree::Storage localStorage;
			if (!storage)
				storage = &localStorage;

			Seq reader { text };
			while (reader.n)
			{
				// Every absolute URI contains a scheme followed by a colon. Find a possible scheme
				reader.DropToFirstByteOfType(Is_scheme_char);
				if (!reader.n)
					break;

				Seq beforeUri = reader.ReadToByte(':');
				if (!reader.n)
					break;

				Seq afterColon = reader;
				afterColon.DropByte();

				if (!Is_scheme_char(beforeUri.LastByte()))
					reader = afterColon;
				else
				{
					// Read backwards over bytes that could be part of the scheme
					bool inSingleQuotes {}, inParentheses {};
					while (true)
					{
						--(beforeUri.n);
						--(reader.p);
						++(reader.n);

						uint b = beforeUri.LastByte();
						if (!Is_scheme_char(b))
						{
							     if (b == '('  ) inParentheses  = true;
							else if (b == '\'' ) inSingleQuotes = true;
							break;
						}
					}

					// We are at the start of a possible scheme and colon. See if it's a valid URI
					Seq uri = FindLeadingUri(reader, storage);
					if (!uri.n)
						reader = afterColon;
					else
					{
						if (inSingleQuotes)
							uri = uri.ReadToByte('\'');
						else if (inParentheses)
						{
							// Read over balanced parentheses in the URI. Stop at first unbalanced closing parenthesis.
							Seq uriReader = uri;
							sizet nrOpenParens {};

							while (true)
							{
								uriReader.DropToFirstByteOf("()");
								uint b = uriReader.FirstByte();
								if (b == UINT_MAX)
									break;

								if (b == '(')
									++nrOpenParens;
								else
								{
									EnsureThrow(b == ')');
									if (!nrOpenParens)
										break;

									--nrOpenParens;
								}

								uriReader.DropByte();
							}

							if (uriReader.n)
							{
								EnsureThrow(uri.n > uriReader.n);
								uri.n -= uriReader.n;
								EnsureThrow(uri.p[uri.n] == ')');
							}
						}

						// We have found the URI
						uris.Add(uri);
						reader.DropBytes(uri.n);
					}
				}
			}
		}

	}
}
