#pragma once

#include "AtNum.h"
#include "AtSatCast.h"
#include "AtStr.h"


namespace At
{

	// TzInfo

	struct TzInfo : TIME_ZONE_INFORMATION
	{
		DWORD m_tzId;
	
		TzInfo();
		LONG CurrentBias() const;
	};



	// TzBias	

	class TzBias
	{
	public:
		TzBias() = default;
		TzBias(int bias) : m_bias(bias) {}
		TzBias(TzInfo const& tzi) : m_bias(tzi.CurrentBias()) {}

		TzBias& Set(TzInfo const& tzi) { m_bias = tzi.CurrentBias(); }

		bool operator== (TzBias x) const noexcept { return m_bias == x.m_bias; }
		bool operator!= (TzBias x) const noexcept { return m_bias != x.m_bias; }

		void EncObj(Enc& s, char const* sym = "+-") const;
		
	private:
		int m_bias {};		// As used by Windows in TIME_ZONE_INFORMATION: difference between UTC and local time in minutes
							// This is the same number as obtained in JavaScript from new Date().getTimezoneOffset()
	};



	// TimeFmt

	struct TimeFmt
	{
		enum EEngDateAbbr { EngDateAbbr };		// "Jan 1, 2019"
		enum EEngDateFull { EngDateFull };	// "January 1, 2019"
		enum EYyyyMmDd    { YyyyMmDd };		// YYYY-mm-dd
		enum EHttp        { Http };
		enum EEmail       { Email };
		enum EIso         { IsoZ=1,
		                    IsoUnitMask=6, IsoSec=0, IsoMilli=2, IsoMicro=4,
						    IsoSecZ   = IsoSec   | IsoZ,
						    IsoMilliZ = IsoMilli | IsoZ,
						    IsoMicroZ = IsoMicro | IsoZ };
		enum EDense       { Dense };
		enum EDuration    { DurationDays, DurationHours, DurationMinutes, DurationSeconds, DurationMilliseconds };
	};



	// Time: A lightweight wrapper around a uint64 to represent time points and time periods.

	class Time
	{
	public:
		// Gets current time in UTC. The return value is the result of GetSystemTimeAsFileTime().
		static Time NonStrictNow();

		// Returns a strictly increasing current time in UTC.
		// Normally, the return value will be the result of GetSystemTimeAsFileTime().
		// However, whenever the result would be lower than, or equal to, a value returned by a previous call,
		// the function will return the previously returned value plus one.
		static Time StrictNow();

		// Converts the current value (assumed to be in UTC) to local time on the current computer.
		Time UtcToLocal() const;

	public:
		enum : uint { SecondsPerDay = 86400U };

		// Operations
		static Time Min            () noexcept { return Time::FromFt(0          ); }
		static Time Max            () noexcept { return Time::FromFt(UINT64_MAX ); }
		static Time MaxDisplayable () noexcept { return Time::FromFt(INT64_MAX  ); }

		bool Any       () const noexcept { return m_ft != 0; }
		bool operator! () const noexcept { return m_ft == 0; }

		void Clear() noexcept { m_ft = 0; }

		Time& operator++ ()             noexcept { m_ft = SatAdd<uint64>(m_ft,      1); return *this; }
		Time& operator-- ()             noexcept { m_ft = SatSub<uint64>(m_ft,      1); return *this; }
		Time& operator+= (Time t)       noexcept { m_ft = SatAdd<uint64>(m_ft, t.m_ft); return *this; }
		Time& operator-= (Time t)       noexcept { m_ft = SatSub<uint64>(m_ft, t.m_ft); return *this; }
		Time& operator*= (uint n)       noexcept { m_ft = SatMul<uint64>(m_ft,      n); return *this; }

		Time  operator++ (int)          noexcept { Time prev{*this}; operator++(); return prev; }
		Time  operator-- (int)          noexcept { Time prev{*this}; operator--(); return prev; }
		Time  operator+  (Time t) const noexcept { return Time(*this).operator+=(t); }
		Time  operator-  (Time t) const noexcept { return Time(*this).operator-=(t); }
		Time  operator*  (uint n) const noexcept { return Time(*this).operator*=(n); }

		bool  operator<  (Time t) const noexcept { return m_ft <  t.m_ft; }
		bool  operator<= (Time t) const noexcept { return m_ft <= t.m_ft; }
		bool  operator== (Time t) const noexcept { return m_ft == t.m_ft; }
		bool  operator!= (Time t) const noexcept { return m_ft != t.m_ft; }
		bool  operator>  (Time t) const noexcept { return m_ft >  t.m_ft; }
		bool  operator>= (Time t) const noexcept { return m_ft >= t.m_ft; }

		uint GetMicrosecondPart() const noexcept { return (m_ft / 10ULL) % 1000000ULL; }
		uint GetMillisecondPart() const noexcept { return (m_ft / 10000ULL) % 1000ULL; }

		Time LastValueInDay () const noexcept { if (m_ft >= FirstValueInLastDay) return Max();
		                                        return FromFt((((m_ft / UnitsPerDay) + 1) * UnitsPerDay) - 1); }
		Time FirstValueInDay() const noexcept { return FromFt(  (m_ft / UnitsPerDay)      * UnitsPerDay     ); }

	public:
		// Conversions from/to numeric time units
		static Time FromMicroseconds (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerMicrosecond>(v); return t; }
		static Time FromMilliseconds (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerMillisecond>(v); return t; }
		static Time FromSeconds      (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerSecond     >(v); return t; }
		static Time FromMinutes      (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerMinute     >(v); return t; }
		static Time FromHours        (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerHour       >(v); return t; }
		static Time FromDays         (uint64 v) noexcept { Time t; t.m_ft = SatMulConst<uint64, UnitsPerDay        >(v); return t; }
							  
		uint64 ToMicroseconds   () const   noexcept { return m_ft / UnitsPerMicrosecond; }
		uint64 ToMilliseconds   () const   noexcept { return m_ft / UnitsPerMillisecond; }
		uint64 ToSeconds        () const   noexcept { return m_ft / UnitsPerSecond     ; }
		uint64 ToMinutes        () const   noexcept { return m_ft / UnitsPerMinute     ; }
		uint64 ToHours          () const   noexcept { return m_ft / UnitsPerHour       ; }
		uint64 ToDays           () const   noexcept { return m_ft / UnitsPerDay        ; }

	public:
		// Conversions from/to other internal representations
		static Time FromFt(uint64 v) noexcept { Time t; t.m_ft = v; return t; }
		static Time FromFt(FILETIME ft) noexcept { uint64 v = (((uint64) ft.dwHighDateTime) << 32U) | ft.dwLowDateTime; return FromFt(v); }
		static Time FromSystemTime(SYSTEMTIME const& st) noexcept;	// Value is zero on failure (!IsValid() && IsMin())
		static Time FromUnixTime(int64 v) noexcept { Time t; t.m_ft = SatCast<uint64>(SatAdd<int64>(UnixTimeOrigin, SatMulConst<int64, 10000000LL>(v))); return t; }

		uint64 ToFt() const noexcept { return m_ft; }
		void   ToSystemTime(SYSTEMTIME& st) const;
		int64  ToUnixTime() const noexcept { return SatSub<int64>(SatCast<int64>(m_ft), UnixTimeOrigin) / 10000000LL; }

	public:
		// Conversions from/to external representations

		// Will read partial times. At least YYYY-MM-DD must be present. Value is zero on failure (!IsValid() && IsMin()).
		static Time ReadIsoStyleTimeStr(Seq& s) noexcept;
		static Time FromIsoStyleTimeStr(Seq s) noexcept { return ReadIsoStyleTimeStr(s); }

		void Enc_EngDateAbbr (Enc& s) const { Enc_DateWithNamedMonth(s, sc_monthNames_englishAbbr); }
		void Enc_EngDateFull (Enc& s) const { Enc_DateWithNamedMonth(s, sc_monthNames_englishFull); }
		void Enc_YyyyMmDd    (Enc& s) const;
		void Enc_Http        (Enc& s) const;
		void Enc_Email       (Enc& s) const;
		void Enc_Iso         (Enc& s, TimeFmt::EIso fmt, char delim=' ') const;
		void Enc_Dense       (Enc& s) const;

		void EncObj(Enc& s, TimeFmt::EEngDateAbbr)       const { Enc_EngDateAbbr (s      ); }
		void EncObj(Enc& s, TimeFmt::EEngDateFull)       const { Enc_EngDateFull (s      ); }
		void EncObj(Enc& s, TimeFmt::EYyyyMmDd)          const { Enc_YyyyMmDd    (s      ); }
		void EncObj(Enc& s, TimeFmt::EHttp)              const { Enc_Http        (s      ); }
		void EncObj(Enc& s, TimeFmt::EEmail)             const { Enc_Email       (s      ); }
		void EncObj(Enc& s, TimeFmt::EIso f, char d=' ') const { Enc_Iso         (s, f, d); }
		void EncObj(Enc& s, TimeFmt::EDense)             const { Enc_Dense       (s      ); }

		void EncObj(Enc& s, TimeFmt::EEngDateAbbr f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }
		void EncObj(Enc& s, TimeFmt::EEngDateFull f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }
		void EncObj(Enc& s, TimeFmt::EYyyyMmDd    f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }
		void EncObj(Enc& s, TimeFmt::EHttp        f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }
		void EncObj(Enc& s, TimeFmt::EEmail       f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }
		void EncObj(Enc& s, TimeFmt::EIso         f, char d, Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f, d); }
		void EncObj(Enc& s, TimeFmt::EDense       f,         Seq dflt) const { if (!Any()) s.Add(dflt); else EncObj(s, f   ); }

		void EncObj(Enc& s, TimeFmt::EDuration duration) const;

	public:
		static char const* const sc_daysOfWeek_englishAbbr[];
		static char const* const sc_monthNames_englishAbbr[];
		static char const* const sc_monthNames_englishFull[];

	private:
		uint64 m_ft {};

		void Enc_DateWithNamedMonth(Enc& s, char const* const* monthNames) const;

		static int64  constexpr UnixTimeOrigin   { 116444736000000000LL };

		static uint64 constexpr UnitsPerMicrosecond {   10ULL };
		static uint64 constexpr UnitsPerMillisecond { 1000ULL * UnitsPerMicrosecond };
		static uint64 constexpr UnitsPerSecond      { 1000ULL * UnitsPerMillisecond };
		static uint64 constexpr UnitsPerMinute      {   60ULL * UnitsPerSecond      };
		static uint64 constexpr UnitsPerHour        {   60ULL * UnitsPerMinute      };
		static uint64 constexpr UnitsPerDay         {   24ULL * UnitsPerHour        };

		static uint64 constexpr FirstValueInLastDay { (UINT64_MAX / UnitsPerDay) * UnitsPerDay };
	};


	
	// Use Time methods and operators. They are already implemented in terms of saturating operations.

	template <> Time SatSub(Time a, Time b) = delete;
	template <> Time SatAdd(Time a, Time b) = delete;
	template <> Time SatMul(Time a, Time b) = delete;



	// Global time/date functions

	void SystemTimeToTm  (SYSTEMTIME const& st, tm& t);

	Seq  DayOfWeekToStr3 (uint dayOfWeek);
	Seq  MonthToStr3     (uint month);

}
