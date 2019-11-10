#pragma once

#include "AtEmailEntities.h"
#include "AtEmailServer.h"
#include "AtSchannel.h"
#include "AtSocket.h"
#include "AtThread.h"
#include "AtWorkPool.h"


namespace At
{

	struct Pop3ServerMsgEntry
	{
		sizet m_nrBytes  {};
		Str   m_uniqueId;		// Must persist across sessions; a unique token permanently associated with each message
		bool  m_present  {};	// Set to false when the POP3 client has deleted a message
	};


	class Pop3ServerAuthCx : public RefCountable
	{
	public:
		virtual bool Pop3ServerAuthCx_GetMsgs       (Vec<Pop3ServerMsgEntry>& msgs) = 0;
		virtual bool Pop3ServerAuthCx_GetMsgContent (sizet msgIndex, Enc& content) = 0;
		virtual bool Pop3ServerAuthCx_Commit        (Vec<Pop3ServerMsgEntry> const& msgs) = 0;
	};



	class Pop3ServerThread;

	class Pop3Server : public EmailServer<Pop3ServerThread>
	{
	protected:
		// Must copy or initialize POP3 server configuration into the provided entity
		virtual void Pop3Server_GetCfg(Pop3ServerCfg& cfg) const = 0;

		virtual void Pop3Server_AddSchannelCerts(Schannel& conn) = 0;

		virtual EmailServerAuthResult Pop3Server_Authenticate(SockAddr const& fromHost, Seq userName, Seq password, Rp<Pop3ServerAuthCx>& authCx) = 0;

	private:
		Pop3ServerCfg m_cfg { Entity::Contained };

		EntVec<EmailSrvBinding> const& EmailServer_GetCfgBindings() override final;

		friend class Pop3ServerThread;
	};

}
