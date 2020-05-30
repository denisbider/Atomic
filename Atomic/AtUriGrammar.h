#pragma once

#include "AtParse.h"

namespace At
{
	namespace Uri
	{
		bool V_domainlabel         (ParseNode& p);

		DECL_RUID(scheme)
		DECL_RUID(SlashSlash)
		DECL_RUID(domainlabel)
		DECL_RUID(toplabel)
		DECL_RUID(segment)
		DECL_RUID(abs_path)
		DECL_RUID(query)
		DECL_RUID(QM_query)
		DECL_RUID(abspath_AndOr_query)
		DECL_RUID(rel_segment)
		DECL_RUID(rel_path)

		DECL_RUID(userinfo)
		DECL_RUID(hostname)
		DECL_RUID(IPv4address)
		DECL_RUID(port)
		DECL_RUID(authority)
		DECL_RUID(server)
		DECL_RUID(reg_name)

		DECL_RUID(net_path)
		DECL_RUID(hier_part)
		DECL_RUID(opaque_part)

		DECL_RUID(fragment)
		DECL_RUID(absoluteURI)
		DECL_RUID(relativeURI)
		DECL_RUID(absURI_optFragment)
		DECL_RUID(URI_reference)

		bool C_domainlabel         (ParseNode& p);
		bool C_abs_path            (ParseNode& p);
		bool C_query               (ParseNode& p);
		bool C_QM_query            (ParseNode& p);
		bool C_abspath_AndOr_query (ParseNode& p);

		bool C_URI_reference       (ParseNode& p);
		bool C_URI_references      (ParseNode& p);

		bool IsValidAbsoluteUri(Seq uri);
		bool IsValidRelativeUri(Seq uri);
		bool IsValidUri(Seq uri);

		// Returns empty Seq if input does not start with a valid URI. The URI must be absolute and may include a hash-fragment.
		Seq FindLeadingUri(Seq s, ParseTree::Storage* storage = nullptr);

		// Finds URIs in provided input. Attempts to correctly delineate URIs within parentheses and single quotes, which are normally valid in URIs.
		// If there's an open parenthesis before the URI, reads over balanced parantheses within the URI, then stops at first unbalanced closing parenthesis.
		// If there's a single quote before the URI, reads until the first single quote within the URI.
		// The 'uris' result vector is NOT cleared before adding to it.
		void FindUrisInText(Seq text, Vec<Seq>& uris, ParseTree::Storage* storage = nullptr);
	}
}
