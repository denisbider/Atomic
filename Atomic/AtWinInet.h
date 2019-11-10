#pragma once

#include "AtException.h"


namespace At
{

	class WinInet : NoCopy
	{
	public:
		~WinInet();
	
		WinInet& Init(DWORD accessType);
		WinInet& Connect(Seq server, INTERNET_PORT port, DWORD flags);
		WinInet& OpenRequest(Seq verb, Seq objName, Seq referrer, DWORD flags);
		WinInet& SendRequest(Seq headers, Seq content);
		WinInet& ReadResponse(Str& response, sizet maxSize);

	private:
		HINTERNET m_hWinInet {};
		HINTERNET m_hConnect {};
		HINTERNET m_hRequest {};
	};

}
