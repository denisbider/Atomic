#pragma once

#include "AtMutex.h"
#include "AtStr.h"
#include "AtTime.h"


namespace At
{

	class TextLog : NoCopy
	{
	public:
		virtual ~TextLog();

		struct Flags { enum E : uint { AutoRollover = 1, CreateUnique = 2 }; };

		void Init(Seq baseName, uint flags);

	protected:
		void WriteEntry(Time now, TzInfo const& tzi, Seq entry);
		static void Enc_EntryTime(Enc& s, Time entryTime, TzInfo const& tzi);

	private:
		Mutex  m_mx;

		uint   m_flags {};
		Str    m_baseName;
		Str    m_logDir;
		TzBias m_curLogTzBias;
		uint   m_curLogDatePart {};
		Str    m_curLogPath;
		HANDLE m_h { INVALID_HANDLE_VALUE };

		void GetLocalTimeRep(Time now, TzInfo const& tzi, uint& datePart, uint& timePart);
		void OpenLog(TzBias tzBias, uint datePart, uint timePart);
	};

}

