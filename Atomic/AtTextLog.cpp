#include "AtIncludes.h"
#include "AtTextLog.h"

#include "AtPath.h"
#include "AtTime.h"
#include "AtWinErr.h"
#include "AtWinStr.h"


namespace At
{

	TextLog::~TextLog()
	{
		if (m_h != INVALID_HANDLE_VALUE)
			CloseHandle(m_h);
	}


	void TextLog::Init(Seq baseName)
	{
		EnsureThrow(m_h == INVALID_HANDLE_VALUE);
		
		Str modulePath(GetModulePath());
		m_baseName = baseName;
		m_logDir = JoinPath(GetDirectoryOfFileName(modulePath), baseName);

		TzInfo tzi;
		TzBias tzBias { tzi };

		uint datePart, timePart;
		GetLocalTimeRep(Time::StrictNow(), tzi, datePart, timePart);

		OpenLog(tzBias, datePart, timePart);
	}


	void TextLog::GetLocalTimeRep(Time now, TzInfo const& tzi, uint& datePart, uint& timePart)
	{
		SYSTEMTIME st;
		now.ToSystemTime(st);

		SYSTEMTIME lst;
		if (!SystemTimeToTzSpecificLocalTime(&tzi, &st, &lst))
			{ LastWinErr e; throw e.Make<>("TextLog: Error in SystemTimeToTzSpecificLocalTime"); }

		datePart = (((uint) lst.wYear) * 10000U) + (((uint) lst.wMonth ) * 100U) + ((uint) lst.wDay);
		timePart = (((uint) lst.wHour) * 10000U) + (((uint) lst.wMinute) * 100U) + ((uint) lst.wSecond);
	}


	void TextLog::OpenLog(TzBias tzBias, uint datePart, uint timePart)
	{
		EnsureThrow(m_h == INVALID_HANDLE_VALUE);

		m_curLogPath.Set(JoinPath(m_logDir, m_baseName)).Ch('-').UInt(datePart, 10, 8).Ch('-').UInt(timePart, 10, 6).Ch('-').Obj(tzBias, "PM").Add(".xml");
		m_curLogTzBias = tzBias;
		m_curLogDatePart = datePart;

		CreateDirectoryIfNotExists(m_logDir, DirSecurity::Restricted_AddFilesOnly);

		m_h = CreateFileW(WinStr(m_curLogPath).Z(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
		if (m_h == INVALID_HANDLE_VALUE)
		{
			DWORD rc = GetLastError();
			throw WinErr<>(rc, Str("Error in CreateFile for ").Add(m_curLogPath));
		}
	}


	void TextLog::WriteEntry(Time now, TzInfo const& tzi, Seq entry)
	{
		Locker locker { m_mx };

		EnsureThrow(m_h != INVALID_HANDLE_VALUE);

		uint datePart, timePart;
		GetLocalTimeRep(now, tzi, datePart, timePart);

		TzBias tzBias { tzi };
		if (tzBias != m_curLogTzBias || datePart != m_curLogDatePart)
		{
			CloseHandle(m_h);
			m_h = INVALID_HANDLE_VALUE;

			OpenLog(tzBias, datePart, timePart);
		}

		DWORD bytesWritten;
		if (!WriteFile(m_h, entry.p, NumCast<DWORD>(entry.n), &bytesWritten, 0))
		{
			DWORD rc = GetLastError();
			throw WinErr<>(rc, Str("Error in WriteFile for ").Add(m_curLogPath));
		}

		if (bytesWritten != entry.n)
			throw StrErr(Str("Unexpected number of bytes written to ").Add(m_curLogPath));
	}


	void TextLog::Enc_EntryTime(Enc& s, Time entryTime, TzInfo const& tzi)
	{
		SYSTEMTIME st;
		entryTime.ToSystemTime(st);

		SYSTEMTIME lst;
		if (!SystemTimeToTzSpecificLocalTime(&tzi, &st, &lst))
			{ LastWinErr e; throw e.Make<>("TextLog: Enc_EntryTime: Error in SystemTimeToTzSpecificLocalTime"); }

		s.ReserveInc(27)
		 .UInt(lst.wYear, 10, 4).UInt(lst.wMonth, 10, 2).UInt(lst.wDay, 10, 2).Ch('.')
		 .UInt(lst.wHour, 10, 2).UInt(lst.wMinute, 10, 2).UInt(lst.wSecond, 10, 2).Ch('.')
		 .UInt(entryTime.GetMicrosecondPart(), 10, 6)
		 .Obj(TzBias(tzi));
	}

}
