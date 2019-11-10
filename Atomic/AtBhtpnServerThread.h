#pragma once

#include "AtBhtpnServer.h"
#include "AtHandleReader.h"
#include "AtHandleWriter.h"
#include "AtRcHandle.h"
#include "AtWebServerThread.h"

namespace At
{
	class BhtpnServerThread : public WebServerThread<BhtpnServer>
	{
	private:
		enum { IoTimeoutMs = 60 * 1000 };

		HandleReader* m_reader {};
		HandleWriter* m_writer {};

		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;
		void SendPreparedResponse(HttpRequest& req, HTTP_RESPONSE* resp) override;

		struct ValidState
		{
			ValidState() : m_ok(true) {}

			bool m_ok;
			Seq m_failReason;

			void Fail(Seq reason) { m_ok = false; m_failReason = reason; }
		};

		struct CookedUrlStorage
		{
			WinStr m_fullUrl;
			WinStr m_host;
			WinStr m_absPath;
			WinStr m_queryString;
		};

		void HttpReq_SetVerb     (HTTP_REQUEST& hreq, ValidState& vs, Seq verb);
		void HttpReq_SetUrl      (HTTP_REQUEST& hreq, ValidState& vs, CookedUrlStorage& cus, Seq rawUrl);
		void HttpReq_SetCookedUrl(HTTP_REQUEST& hreq, ValidState& vs, CookedUrlStorage& cus, Seq canonUrl);
		void HttpReq_SetHeader   (HTTP_REQUEST& hreq, ValidState& vs, Vec<HTTP_UNKNOWN_HEADER>& uhdrs, Seq name, Seq value);
		
		struct EnumInstr { enum E { Continue, Stop }; };
		EnumInstr::E EnumKnownCommonHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f);
		void EnumKnownRequestHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f);
		void EnumKnownResponseHeaders(std::function<EnumInstr::E (Seq, HTTP_HEADER_ID)> f);
		void EnumResponseHeaders(HTTP_RESPONSE const& resp, std::function<void (Seq, Seq)> f);

		bool GetKnownRequestHeaderId(HTTP_HEADER_ID& id, Seq name);
		void SendChunkFromFile(HANDLE fileHandle, HTTP_BYTE_RANGE const& byteRange);
		void SendResponseBodyChunk(Seq chunk);
	};
}
