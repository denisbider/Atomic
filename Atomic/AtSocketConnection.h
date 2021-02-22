#pragma once

#include "AtRp.h"
#include "AtSchannel.h"
#include "AtSocket.h"
#include "AtSocketReader.h"
#include "AtSocketWriter.h"


namespace At
{

	struct SocketConnection : RefCountable
	{
		Socket       m_sk;
		SocketReader m_reader;
		SocketWriter m_writer;
		Schannel     m_conn;

		void OnSocketConnected();
	};

}
