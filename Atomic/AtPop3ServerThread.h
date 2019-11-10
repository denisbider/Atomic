#pragma once

#include "AtPop3Server.h"
#include "AtWorkPoolThread.h"
#include "AtWriter.h"


namespace At
{

	enum class Pop3ReplyType { Err, OK };


	class Pop3ServerThread : public WorkPoolThread<Pop3Server>
	{
	private:
		void WorkPoolThread_ProcessWorkItem(void* pvWorkItem) override;

		void SendSingleLineReply(Writer& writer, Pop3ReplyType replyType, Seq text);

		// Reply type is assumed to be +OK. First line follows +OK, subsequent lines are .-escaped and followed up with a trailing CRLF-dot-CRLF
		void SendMultiLineReply(Writer& writer, Seq text);
	};

}
