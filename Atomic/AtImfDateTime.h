#pragma once

#include "AtImfGrammar.h"
#include "AtImfMsgWriter.h"
#include "AtTime.h"


namespace At
{
	namespace Imf
	{

		struct DayMonthYear
		{
			uint16 m_day;
			uint16 m_month;
			uint16 m_year;

			void Read(ParseNode const& dateNode);
		};


		struct HourMinSecTz
		{
			uint16 m_hour;
			uint16 m_minute;
			uint16 m_second;
			int16  m_tzBias;	// Time zone bias from UTC, in minutes, positive or negative

			void Read(ParseNode const& timeNode);
		};


		struct DateTime
		{
			Time m_t;

			DateTime() {}
			DateTime(Time t) : m_t(t) {}

			void Read(ParseNode const& dateTimeNode);
			void Write(MsgWriter& writer) const;
		};

	}
}

