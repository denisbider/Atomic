#include "AtIncludes.h"
#include "AtSocketConnection.h"


namespace At
{

	void SocketConnection::OnSocketConnected()
	{
		m_reader.SetSocket(m_sk.GetSocket());
		m_writer.SetSocket(m_sk.GetSocket());
		m_conn.SetReader(&m_reader);
		m_conn.SetWriter(&m_writer);
	}

}
