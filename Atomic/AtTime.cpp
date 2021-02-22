#include "AtIncludes.h"
#include "AtTime.h"

#include "AtDllKernel32.h"
#include "AtException.h"
#include "AtNumCvt.h"
#include "AtSpinLock.h"
#include "AtWinErr.h"

#pragma warning (push)
#pragma warning (disable: 4073)		// L3: initializers put in library initialization area
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	// TzInfo

	TzInfo::TzInfo()
	{
		m_tzId = GetTimeZoneInformation(this);
		if (m_tzId == TIME_ZONE_ID_INVALID)
			{ LastWinErr e; throw e.Make<>("TzInfo: Error in GetTimeZoneInformation"); }
	}


	LONG TzInfo::CurrentBias() const
	{
		if (m_tzId == TIME_ZONE_ID_STANDARD) return Bias + StandardBias;
		if (m_tzId == TIME_ZONE_ID_DAYLIGHT) return Bias + DaylightBias;
		return Bias;
	}



	// TzBias

	void TzBias::EncObj(Enc& s, char const* sym) const
	{
		unsigned int uBias;

		// Note that sign for bias is inverted in Windows representation, as well as in the result of JavaScript new Date().getTimezoneOffset()
		if (m_bias <= 0)
			uBias = (unsigned int) (-m_bias);
		else
		{
			uBias = (unsigned int) m_bias;
			++sym;
		}

		uBias = ((uBias / 60) * 100) + (uBias % 60);
		s.Ch(*sym).UInt(uBias, 10, 4);
	}



	// Time

	Time Time::NonStrictNow()
	{
		Time t;

		FuncType_GetSystemTimePreciseAsFileTime getSystemTimePreciseAsFileTime = GetFunc_GetSystemTimePreciseAsFileTime();
		if (getSystemTimePreciseAsFileTime)
			getSystemTimePreciseAsFileTime((LPFILETIME) &t.m_ft);
		else
			GetSystemTimeAsFileTime((LPFILETIME) &t.m_ft);

		return t;
	}


	namespace
	{
		// These global objects MUST be initialized before code that may call Time::StrictNow().
		SpinLock a_strictNowLock;
		uint64 a_strictNowLastRet = 0;
	}

	Time Time::StrictNow()
	{
		Time t { NonStrictNow() };
		a_strictNowLock.Acquire();
		
		if (t.m_ft <= a_strictNowLastRet)
			t.m_ft = a_strictNowLastRet + 1;
		a_strictNowLastRet = t.m_ft;

		a_strictNowLock.Release();
		return t;
	}


	Time Time::UtcToLocal() const
	{
		Time t;
		if (!FileTimeToLocalFileTime((FILETIME const*) &m_ft, (LPFILETIME) &t.m_ft))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": FileTimeToLocalFileTime"); }
		return t;
	}


	Time Time::FromSystemTime(SYSTEMTIME const& st) noexcept
	{
		Time t;
		if (!SystemTimeToFileTime(&st, (LPFILETIME) &t.m_ft))
			t.m_ft = 0;
		return t;
	}


	void Time::ToSystemTime(SYSTEMTIME& st) const
	{
		if (!FileTimeToSystemTime((FILETIME const*) &m_ft, &st))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": Error in FileTimeToSystemTime"); }
	}


	SYSTEMTIME const& Time::ToSystemTimeOpt(Opt<SYSTEMTIME>& sto) const
	{
		if (sto.Any())
			return sto.Ref();

		SYSTEMTIME& st = sto.Init();
		ToSystemTime(st);
		return st;
	}


	bool Time::ReadIsoStyleTimeStr(Seq& reader, Time& x) noexcept
	{
		x = Time();

		SYSTEMTIME st {};
		uint64 microseconds {};

		Seq rdr = reader;
		if (rdr.n >= 4)
		{
			Seq field { rdr.p, 4 };
			st.wYear = field.ReadNrUInt16Dec();
			if (!field.n && st.wYear >= 1601 && rdr.DropBytes(4).StripPrefixExact("-") && rdr.n >= 2)
			{
				field = Seq(rdr.p, 2);
				st.wMonth = field.ReadNrUInt16Dec();
				if (!field.n && st.wMonth >= 1 && st.wMonth <= 12 && rdr.DropBytes(2).StripPrefixExact("-") && rdr.n >= 2)
				{
					field = Seq(rdr.p, 2);
					st.wDay = field.ReadNrUInt16Dec();
					if (!field.n && st.wDay >= 1 && st.wDay <= 31)
					{
						rdr.DropBytes(2);
						if ((rdr.StripPrefixExact(" ") || rdr.StripPrefixInsensitive("T")) && rdr.n >= 2)
						{
							field = Seq(rdr.p, 2);
							st.wHour = field.ReadNrUInt16Dec();
							if (field.n || st.wHour > 23)
								st.wHour = 0;
							else if (rdr.DropBytes(2).StripPrefixExact(":") && rdr.n >= 2)
							{
								field = Seq(rdr.p, 2);
								st.wMinute = field.ReadNrUInt16Dec();
								if (field.n || st.wMinute > 59)
									st.wMinute = 0;
								else if (rdr.DropBytes(2).StripPrefixExact(":") && rdr.n >= 2)
								{
									field = Seq(rdr.p, 2);
									st.wSecond = field.ReadNrUInt16Dec();
									if (field.n || st.wSecond > 59)
										st.wSecond = 0;
									else if (rdr.DropBytes(2).StripPrefixExact(".") && rdr.n >= 3)
									{
										field = Seq(rdr.p, 3);
										st.wMilliseconds = field.ReadNrUInt16Dec();
										if (field.n)
											st.wMilliseconds = 0;
										else if (rdr.DropBytes(3).n >= 3)
										{
											field = Seq(rdr.p, 3);
											microseconds = field.ReadNrUInt16Dec();
											if (field.n)
												microseconds = 0;
											else
												rdr.DropBytes(3);
										}
									}
								}
							}
						}

						// We have at least YYYY-MM-DD, and possibly other components.
						x = FromSystemTime(st) + Time::FromMicroseconds(microseconds);
						reader = rdr;
						return true;
					}
				}
			}
		}

		return false;
	}


	char const* const Time::sc_daysOfWeek_englishAbbr[] =
		{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

	char const* const Time::sc_monthNames_englishAbbr[] =
		{ "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	char const* const Time::sc_monthNames_englishFull[] =
		{ "", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };


	void Time::Enc_DateWithNamedMonth(Enc& s, Opt<SYSTEMTIME>& sto, char const* const* monthNames) const
	{
		s.ReserveInc(9 + 1 + 2 + 2 + 4);	// "March 6, 2019"

		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		if (st.wMonth < 1 || st.wMonth > 12) throw ErrWithCode<>(st.wMonth, __FUNCTION__ ": Unexpected month");
		s.Add(monthNames[st.wMonth]).Ch(' ').UInt(st.wDay).Add(", ").UInt(st.wYear);
	}


	void Time::Enc_YyyyMmDd(Enc& s, Opt<SYSTEMTIME>& sto) const
	{
		s.ReserveInc(4 + 1 + 2 + 1 + 2);	// "2019-03-06"

		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s.UInt(st.wYear, 10, 4).Ch('-').UInt(st.wMonth, 10, 2).Ch('-').UInt(st.wDay, 10, 2);
	}


	void Time::Enc_Time24(Enc& s, Opt<SYSTEMTIME>& sto) const
	{
		s.ReserveInc(2 + 1 + 2);			// "09:07"

		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s.UInt(st.wHour, 10, 2).Ch(':').UInt(st.wMinute, 10, 2);
	}


	void Time::Enc_Http(Enc& s, Opt<SYSTEMTIME>& sto) const
	{
		s.ReserveInc(29);	// "Wed, 06 Mar 2019 23:59:59 GMT"

		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s.Add(DayOfWeekToStr3(st.wDayOfWeek))
			.Add(", ").UInt(st.wDay, 10, 2)
			.Ch(' ').Add(MonthToStr3(st.wMonth))
			.Ch(' ').UInt(st.wYear, 10, 4)
			.Ch(' ').UInt(st.wHour, 10, 2)
			.Ch(':').UInt(st.wMinute, 10, 2)
			.Ch(':').UInt(st.wSecond, 10, 2)
			.Add(" GMT");
	}


	void Time::Enc_Email(Enc& s, Opt<SYSTEMTIME>& sto) const
	{
		s.ReserveInc(TimeFmt::Email_MaxBytes);	// "Wed, 16 Mar 2019 23:59:59 +0000"

		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s.Add(DayOfWeekToStr3(st.wDayOfWeek))
			.Add(", ").UInt(st.wDay)
			.Ch(' ').Add(MonthToStr3(st.wMonth))
			.Ch(' ').UInt(st.wYear)
			.Ch(' ').UInt(st.wHour,   10, 2)
			.Ch(':').UInt(st.wMinute, 10, 2)
			.Ch(':').UInt(st.wSecond, 10, 2)
			.Add(" +0000");
	}


	void Time::Enc_Iso(Enc& s, Opt<SYSTEMTIME>& sto, TimeFmt::EIso fmt, char delim) const
	{
		s.ReserveInc(27);	// "2019-03-06 23:59:59.000000Z"
		
		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s			.UInt(st.wYear,   10, 4)
			.Ch('-').UInt(st.wMonth,  10, 2)
			.Ch('-').UInt(st.wDay,    10, 2)
			.Ch(delim).UInt(st.wHour, 10, 2)
			.Ch(':').UInt(st.wMinute, 10, 2)
			.Ch(':').UInt(st.wSecond, 10, 2);

		int unit { fmt & TimeFmt::IsoUnitMask };
		if (unit != TimeFmt::IsoSec)
		{
			s.Ch('.');

			if (unit == TimeFmt::IsoMilli)
			{
				uint64 milliseconds { (m_ft / 10000) % 1000 };
				s.UInt(milliseconds, 10, 3);
			}
			else if (unit == TimeFmt::IsoMicro)
			{
				uint64 microseconds { (m_ft / 10) % 1000000 };
				s.UInt(microseconds, 10, 6);
			}
			else
				EnsureThrow(!"Unrecognized ISO time format value");
		}

		if ((fmt & TimeFmt::IsoZ) == TimeFmt::IsoZ)
			s.Ch('Z');
	}


	void Time::Enc_Dense(Enc& s, Opt<SYSTEMTIME>& sto) const
	{
		s.ReserveInc(15);	// "20190306-235959"
		
		SYSTEMTIME const& st = ToSystemTimeOpt(sto);

		s	.UInt(st.wYear,   10, 4)
			.UInt(st.wMonth,  10, 2)
			.UInt(st.wDay,    10, 2).Ch('-')
			.UInt(st.wHour,   10, 2)
			.UInt(st.wMinute, 10, 2)
			.UInt(st.wSecond, 10, 2);
	}


	void Time::EncObj(Enc& s, TimeFmt::EDuration duration) const
	{
		struct DescPart
		{
			DescPart(uint64 value, char const* period) : m_value(value), m_period(period) {}

			uint64      m_value;
			char const* m_period;
		};

		char const* smallestPeriod = "";
		VecFix<DescPart, 5> descParts;
		uint64 v = m_ft;

		auto maybeAddDescPart = [&smallestPeriod, &descParts, &v] (uint64 unitsPerPeriod, char const* periodSingular, char const* periodPlural)
			{
				smallestPeriod = periodSingular;
				if (descParts.Any() || v > unitsPerPeriod)
				{
					uint64 n = v / unitsPerPeriod;
					if (n == 1) descParts.Add(n, periodSingular);
					else        descParts.Add(n, periodPlural);
					v -= (n * unitsPerPeriod);
				}
			};

		                                               maybeAddDescPart(UnitsPerDay,         "day",    "days"    );
		if (duration >= TimeFmt::DurationHours)        maybeAddDescPart(UnitsPerHour,        "hour",   "hours"   );
		if (duration >= TimeFmt::DurationMinutes)      maybeAddDescPart(UnitsPerMinute,      "minute", "minutes" );
		if (duration >= TimeFmt::DurationSeconds)      maybeAddDescPart(UnitsPerSecond,      "second", "seconds" );
		if (duration >= TimeFmt::DurationMilliseconds) maybeAddDescPart(UnitsPerMillisecond, "ms",     "ms"      );

		if (!descParts.Any())
			s.Add("less than 1 ").Add(smallestPeriod);
		else
		{
			for (sizet i=0; i!=descParts.Len(); ++i)
			{
				if (i != 0)
					if (i == descParts.Len() - 1)
						s.Add(" and ");
					else
						s.Add(", ");

				s.UInt(descParts[i].m_value).Add(" ").Add(descParts[i].m_period);
			}
		}
	}



	// General time/date functions

	void SystemTimeToTm(SYSTEMTIME const& st, tm& t)
	{
		t.tm_sec = st.wSecond;
		t.tm_min = st.wMinute;
		t.tm_hour = st.wHour;
		t.tm_mday = st.wDay;
		t.tm_mon = st.wMonth - 1;
		t.tm_year = st.wYear - 1900;
		t.tm_wday = st.wDayOfWeek;
		t.tm_yday = 0;
		t.tm_isdst = 0;
	}


	Seq DayOfWeekToStr3(uint dayOfWeek)
	{
		if (dayOfWeek > 6) throw ErrWithCode<>(dayOfWeek, __FUNCTION__ ": Unexpected day of week");
		return Time::sc_daysOfWeek_englishAbbr[dayOfWeek];
	}


	Seq MonthToStr3(uint month)
	{
		if (month < 1 || month > 12) throw ErrWithCode<>(month, __FUNCTION__ ": Unexpected month");
		return Time::sc_monthNames_englishAbbr[month];
	}
}
