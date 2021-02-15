#pragma once

#include "AtSmtpReceiver.h"
#include "AtWorkPoolThread.h"
#include "AtWriter.h"


namespace At
{

	class SmtpReceiverThread : public WorkPoolThread<SmtpReceiver>
	{
	private:
		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;

		EhloHost ParseEhloHost(Seq host);

		bool ReadAuthLoginParameter(Seq& paramReader, Str& result);
		bool ReadAuthLoginResponse(Schannel& conn, Str& result);

		void SendReply(Writer& writer, uint code, Seq text);
		void SendNegativeReply(Writer& writer, uint code, Seq text);

		Str SanitizeAndQuoteRemoteString(Seq s, sizet maxBytes);
	};

}
