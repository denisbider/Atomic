#pragma once

#include "AtUriGrammar.h"

namespace At
{
	namespace Bhtpn
	{
		DECL_RUID(scheme_prefix)
		DECL_RUID(Dot_l)
		DECL_RUID(Dot_r)

		DECL_RUID(pipename)
		DECL_RUID(servername)
		DECL_RUID(authority_local)
		DECL_RUID(authority_remote)
		DECL_RUID(authority)

		DECL_RUID(url)

		bool C_url(ParseNode& p);
	}
}
