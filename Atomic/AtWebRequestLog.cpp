#include "AtIncludes.h"
#include "AtWebRequestLog.h"

#include "AtHttpVerb.h"


namespace At
{

	void WebRequestLog::OnRequest(HttpRequest& req)
	{
		TzInfo tzi;
		Str entry;
		entry.ReserveExact(1000)
			 .Add("<request id=\"") .UInt(req.RequestId(), 16)
			 .Add("\" time=\"")     .Fun(Enc_EntryTime, req.RequestTime(), tzi)
			 .Add("\" addr=\"")     .Obj(req.RemoteAddr(), SockAddr::AddrPort)
			 .Add("\" verb=\"")     .Add(HttpVerb::Describe(req->Verb))
			 .Add("\" len=\"")      .UInt(req.BodySize())
			 .Add("\"\r\n url=\"")  .HtmlAttrValue(req.FullUrl(), Html::CharRefs::Escape)
			 .Add("\"\r\n ref=\"")  .HtmlAttrValue(req.Referrer(), Html::CharRefs::Escape)
			 .Add("\" />\r\n");

		WriteEntry(req.RequestTime(), tzi, entry);
	}


	void WebRequestLog::OnResponse(HttpRequest const& req, HTTP_RESPONSE* resp, bool haveContentLength, uint64 contentLength)
	{
		Time now { Time::StrictNow() };
		TzInfo tzi;

		Str entry;
		entry.ReserveExact(1000)
			 .Add("<response reqId=\"").UInt(req.RequestId(), 16)
			 .Add("\" time=\"").Fun(Enc_EntryTime, now, tzi)
			 .Add("\" elapsed=\"").UInt((now - req.RequestTime()).ToFt() / 10ULL)
			 .Add("\" status=\"").UInt(resp->StatusCode)
			 .Add("\" len=\"");

		if (haveContentLength)
			entry.UInt(contentLength);
		else
			entry.Add("?");

		entry.Add("\" />\r\n");

		WriteEntry(now, tzi, entry);
	}

}
