#pragma once

#include "AtAscii.h"
#include "AtParse.h"

namespace At
{
	namespace Smtp
	{
		DECL_RUID(sub_domain)
		DECL_RUID(Domain)

		DECL_RUID(AddrLit)
		DECL_RUID(Domain_or_AddrLit)
		DECL_RUID(Mailbox)
		DECL_RUID(Path)
		DECL_RUID(EmptyPath)
		DECL_RUID(Reverse_path)
		DECL_RUID(SizeParamVal)
		DECL_RUID(MailParams)

		bool C_Domain            (ParseNode& p);
		bool C_AddrLit           (ParseNode& p);
		bool C_Local_part        (ParseNode& p);
		bool C_Domain_or_AddrLit (ParseNode& p);
		bool C_Path              (ParseNode& p);
		bool C_Reverse_path      (ParseNode& p);
		bool C_MailParams        (ParseNode& p);
	}
}
