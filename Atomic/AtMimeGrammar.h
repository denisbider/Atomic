#pragma once

#include "AtParse.h"

namespace At
{
	namespace Mime
	{
		DECL_RUID(kw_content_type)
		DECL_RUID(kw_content_enc)
		DECL_RUID(kw_content_id)
		DECL_RUID(kw_content_desc)
		DECL_RUID(kw_content_disp)
		DECL_RUID(kw_mime_version)
		DECL_RUID(extension_field_name)

		DECL_RUID(token_text)
		DECL_RUID(param_name)
		DECL_RUID(param_value)
		DECL_RUID(parameter)
		DECL_RUID(ct_type)
		DECL_RUID(ct_subtype)
		DECL_RUID(content_type)
		DECL_RUID(content_enc)
		DECL_RUID(content_id)
		DECL_RUID(content_desc)
		DECL_RUID(disposition_type)
		DECL_RUID(content_disp)
		DECL_RUID(number_text)
		DECL_RUID(version_major)
		DECL_RUID(version_minor)
		DECL_RUID(mime_version)
		DECL_RUID(extension_field)
		DECL_RUID(part_header)

		DECL_RUID(dash_boundary)
		DECL_RUID(close_delimiter)
		DECL_RUID(prologue)
		DECL_RUID(epilogue)
		DECL_RUID(transport_padding)
		DECL_RUID(part_content)
		DECL_RUID(body_part)
		DECL_RUID(multipart_body)

		bool C_token              (ParseNode& p);
		bool C_parameter          (ParseNode& p);
		bool C_content_type_inner (ParseNode& p);
		bool C_content_type       (ParseNode& p);
		bool C_msg_header_field   (ParseNode& p);
		bool C_multipart_body     (ParseNode& p);
	}
}
