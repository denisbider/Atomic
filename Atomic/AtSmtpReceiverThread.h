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

		void SendReply(Writer& writer, uint code, Seq text);
		void SendNegativeReply(Writer& writer, uint code, Seq text);
	};

}
