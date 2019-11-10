#pragma once

#include "AtAscii.h"
#include "AtParse.h"


namespace At
{
	namespace Dsn
	{

		DECL_RUID(action_value)
		DECL_RUID(av_failed)
		DECL_RUID(av_delayed)
		DECL_RUID(av_delivered)
		DECL_RUID(av_relayed)
		DECL_RUID(av_expanded)

		DECL_RUID(kw_action)
		DECL_RUID(kw_arrival_date)
		DECL_RUID(kw_diag_code)
		DECL_RUID(kw_dsn_gateway)
		DECL_RUID(kw_final_rcpt)
		DECL_RUID(kw_final_log_id)
		DECL_RUID(kw_last_attempt)
		DECL_RUID(kw_orig_envl_id)
		DECL_RUID(kw_orig_rcpt)
		DECL_RUID(kw_rcvd_from_mta)
		DECL_RUID(kw_remote_mta)
		DECL_RUID(kw_reporting_mta)
		DECL_RUID(kw_status)
		DECL_RUID(kw_will_retry)

		DECL_RUID(action)
		DECL_RUID(arrival_date)
		DECL_RUID(diag_code)
		DECL_RUID(dsn_gateway)
		DECL_RUID(ext_field_name)
		DECL_RUID(extension)
		DECL_RUID(final_rcpt)
		DECL_RUID(final_log_id)
		DECL_RUID(last_attempt)
		DECL_RUID(orig_envl_id)
		DECL_RUID(orig_rcpt)
		DECL_RUID(rcvd_from_mta)
		DECL_RUID(remote_mta)
		DECL_RUID(reporting_mta)
		DECL_RUID(status_code)
		DECL_RUID(status)
		DECL_RUID(will_retry_until)
		
		DECL_RUID(per_msg_fields)
		DECL_RUID(per_rcpt_fields)

		bool C_dsn_content(ParseNode& p);

	}
}
