#include "AtIncludes.h"
#include "AtMimeTypes.h"

namespace At
{
	namespace Mime
	{
		Seq FileExtToMimeType(Seq extension)
		{
			// Text
			if (extension.EqualInsensitive("c"   )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("cpp" )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("cs"  )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("css" )) return "text/css; charset=UTF-8";
			if (extension.EqualInsensitive("h"   )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("htm" )) return "text/html; charset=UTF-8";
			if (extension.EqualInsensitive("html")) return "text/html; charset=UTF-8";
			if (extension.EqualInsensitive("js"  )) return "text/javascript; charset=UTF-8";
			if (extension.EqualInsensitive("ps1" )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("txt" )) return "text/plain; charset=UTF-8";
			if (extension.EqualInsensitive("xml" )) return "text/xml; charset=UTF-8";

			// Images
			if (extension.EqualInsensitive("gif" )) return "image/gif";
			if (extension.EqualInsensitive("ico" )) return "image/x-icon";		// See note about ICO and image/x-icon
			if (extension.EqualInsensitive("jpeg")) return "image/jpeg";
			if (extension.EqualInsensitive("jpg" )) return "image/jpeg";
			if (extension.EqualInsensitive("png" )) return "image/png";
			if (extension.EqualInsensitive("svg" )) return "image/svg+xml";

			// Application
			if (extension.EqualInsensitive("pdf" )) return "application/pdf";
			if (extension.EqualInsensitive("rss" )) return "application/rss+xml; charset=UTF-8";

			return "application/octet-stream";

			// Note about ICO and image/x-icon:
			//
			// https://blogs.msdn.microsoft.com/ieinternals/2011/02/11/ie9-rc-minor-changes-list/#comment-1108
			//
			// Eric Law, February 11, 2011: "We use "image/x-icon" because that's the MIME type we've always used.
			// Someone at some point (AFAIK, not related to Microsoft) proposed registration of the MIME type as
			// "vnd.microsoft.icon", but Windows doesn't actually use that, it uses image/x-icon."
		}
	}
}
