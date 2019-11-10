#include "AtIncludes.h"
#include "AtBhtpnGrammar.h"

namespace At
{
	namespace Bhtpn
	{
		DEF_RUID_B(scheme_prefix)
		DEF_RUID_B(Dot_l)
		DEF_RUID_B(Dot_r)

		DEF_RUID_B(pipename)
		DEF_RUID_B(servername)
		DEF_RUID_B(authority_local)
		DEF_RUID_B(authority_remote)
		DEF_RUID_B(authority)

		DEF_RUID_B(url)

		bool C_scheme_prefix       (ParseNode& p) { return Parse::G_SeqMatchInsens (p, id_scheme_prefix,     "bhtpn://"                                                ); }
		bool C_Dot_l               (ParseNode& p) { return Parse::G_SeqMatchInsens (p, id_Dot_l,             ".l"                                                      ); }
		bool C_Dot_r               (ParseNode& p) { return Parse::G_SeqMatchInsens (p, id_Dot_l,             ".r"                                                      ); }

		bool C_pipename            (ParseNode& p) { return Parse::G_Req            (p, id_pipename,          Uri::V_domainlabel                                        ); }
		bool C_Dot_domainlabel     (ParseNode& p) { return Parse::G_Req<1,1>       (p, id_Append,            Parse::C_Dot, Uri::C_domainlabel                          ); }
		bool C_servername          (ParseNode& p) { return Parse::G_Req<1,0>       (p, id_servername,        Uri::C_domainlabel, Parse::G_Repeat<C_Dot_domainlabel>    ); }
		bool C_authority_local     (ParseNode& p) { return Parse::G_Req<1,1>       (p, id_authority_local,   C_pipename, C_Dot_l                                       ); }
		bool C_authority_remote    (ParseNode& p) { return Parse::G_Req<1,1,1,1>   (p, id_authority_remote,  C_pipename, Parse::C_Dot, C_servername, C_Dot_r           ); }
		bool C_authority           (ParseNode& p) { return Parse::G_Choice         (p, id_authority,         C_authority_local, C_authority_remote                     ); }
																				   
		bool C_url                 (ParseNode& p) { return Parse::G_Req<1,1,0>     (p, id_url,               C_scheme_prefix, C_authority, Uri::C_abspath_AndOr_query  ); }
	}
}
