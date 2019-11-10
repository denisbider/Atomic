#pragma once

#include "AtEmailEntities.h"
#include "AtReader.h"
#include "AtWorkPool.h"


namespace At
{

	// EmailServer

	struct EmailServerWorkItem
	{
		Socket   m_sk;
		SockAddr m_saRemote;
		Str      m_bindingToken;
	};

	
	enum class EmailServerAuthResult { Success, InvalidCredentials, AttemptsTooFrequent, TransactionFailed };


	template <class ThreadType>
	class EmailServer : public WorkPool<ThreadType, EmailServerWorkItem>
	{
	public:
		// Usually after the email server has been started, signals it to reload bindings after a configuration change
		void ReloadBindings() { m_reloadBindingsTrigger.Signal(); }

	protected:
		virtual EntVec<EmailSrvBinding> const& EmailServer_GetCfgBindings() = 0;

	private:
		Event m_reloadBindingsTrigger { Event::CreateAuto };

		void WorkPool_Run() override final;
	};



	// Timeouts and limits

	enum
	{
		// For SMTP, RFC 5321 prescribes 5 minute timeouts.
		// However, Yahoo uses 15 seconds before MAIL, and 60 seconds thereafter, and it seems to work fine for them.
		// Hotmail uses 60 seconds before MAIL, haven't tested for timeouts after.
		EmailServer_RecvTimeoutMs = 60 * 1000,
		EmailServer_SendTimeoutMs = 60 * 1000,

		EmailServer_MaxClientLineBytes = 1000,
	};


	// EmailServer_Disconnect

	struct EmailServer_Disconnect : public StrErr
	{
		EmailServer_Disconnect(Seq msg) : StrErr(msg) {}
	};



	// EmailServer_ClientLine

	struct EmailServer_ClientLine
	{
		Str m_line;

		void ReadLine(Reader& reader);	
	};



	// EmailServer_ClientCmd

	struct EmailServer_ClientCmd : EmailServer_ClientLine
	{
		Str m_cmd;
		Str m_params;

		void ReadCmd(Reader& reader);	
	};

}
