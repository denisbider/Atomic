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
	}
}
