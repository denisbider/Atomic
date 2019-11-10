#include "AtIncludes.h"
#include "AtWinInet.h"

#include "AtNumCvt.h"
#include "AtWinErr.h"
#include "AtWinStr.h"

namespace At
{
	WinInet::~WinInet()
	{
		if (m_hRequest) EnsureReportWithCode(InternetCloseHandle(m_hRequest), GetLastError());
		if (m_hConnect) EnsureReportWithCode(InternetCloseHandle(m_hConnect), GetLastError());
		if (m_hWinInet) EnsureReportWithCode(InternetCloseHandle(m_hWinInet), GetLastError());
	}

	
	WinInet& WinInet::Init(DWORD accessType)
	{
		EnsureThrow(!m_hWinInet);

		m_hWinInet = InternetOpenW(L"Atomic", accessType, 0, 0, 0);
		if (!m_hWinInet)
			{ LastWinErr e; throw e.Make<>("AtWinInet: Error in InternetOpen"); }

		return *this;
	}


	WinInet& WinInet::Connect(Seq server, INTERNET_PORT port, DWORD flags)
	{
		EnsureThrow(m_hWinInet);
	
		if (m_hConnect)
		{
			if (!InternetCloseHandle(m_hConnect))
				{ LastWinErr e; throw e.Make<>("AtWinInet: Error in InternetCloseHandle(m_hConnect)"); }
		
			m_hConnect = 0;
		}

		m_hConnect = InternetConnectW(m_hWinInet, WinStr(server).Z(), port, 0, 0, INTERNET_SERVICE_HTTP, flags, 0);
		if (!m_hConnect)
			{ LastWinErr e; throw e.Make<>("AtWinInet: Error in InternetConnect"); }

		return *this;
	}


	WinInet& WinInet::OpenRequest(Seq verb, Seq objName, Seq referrer, DWORD flags)
	{
		EnsureThrow(m_hWinInet);
		EnsureThrow(m_hConnect);
	
		if (m_hRequest )
		{
			if (!InternetCloseHandle(m_hRequest))
				{ LastWinErr e; throw e.Make<>("AtWinInet: Error in InternetCloseHandle(m_hRequest)"); }
		
			m_hRequest = 0;
		}
	
		m_hRequest = HttpOpenRequestW(m_hConnect, WinStr(verb).Z(), WinStr(objName).Z(), nullptr, If(referrer.n, wchar_t const*, WinStr(referrer).Z(), nullptr), nullptr, flags, 0);
		if (!m_hRequest)
			{ LastWinErr e; throw e.Make<>("AtWinInet: Error in HttpOpenRequest"); }

		return *this;
	}


	WinInet& WinInet::SendRequest(Seq headers, Seq content)
	{
		EnsureThrow(m_hWinInet);
		EnsureThrow(m_hConnect);
		EnsureThrow(m_hRequest);
	
		WinStr headersW(headers);
		if (!HttpSendRequestW(m_hRequest,
				If(headers.n, wchar_t const*, headersW.Z(), nullptr), NumCast<DWORD>(ZLen(headersW.Z())),
				If(content.n, LPVOID, (LPVOID) content.p, nullptr), NumCast<DWORD>(content.n)))
			{ LastWinErr e; throw e.Make<>("AtWinInet: Error in HttpSendRequest"); }
	
		return *this;
	}


	WinInet& WinInet::ReadResponse(Str& response, sizet maxSize)
	{
		EnsureThrow(m_hWinInet);
		EnsureThrow(m_hConnect);
		EnsureThrow(m_hRequest);
	
		enum { MaxReadChunkSize = 16000 };
		DWORD readChunkSize = NumCast<DWORD>(PickMin<sizet>(maxSize, MaxReadChunkSize));

		response.Clear();
		response.ReserveExact(readChunkSize);
	
		sizet totalBytesRead = 0;
		while (true)
		{
			response.Resize(totalBytesRead + readChunkSize);
		
			DWORD bytesRead;
			if (!InternetReadFile(m_hRequest, response.Ptr() + totalBytesRead, readChunkSize, &bytesRead))
				{ LastWinErr e; throw e.Make<>("AtWinInet: Error in InternetReadFile"); }

			totalBytesRead += bytesRead;
			response.Resize(totalBytesRead);
		
			if (!bytesRead)
				break;
		}
	
		return *this;
	}
}
