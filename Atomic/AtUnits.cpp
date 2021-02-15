#include "AtIncludes.h"
#include "AtUnits.h"


namespace At
{
	namespace Units
	{

		Unit Bytes[Bytes_NrUnits] = {
				{                            1ULL,  "B" },
				{                         1024ULL, "kB" },
				{                 1024ULL*1024ULL, "MB" },
				{         1024ULL*1024ULL*1024ULL, "GB" },
				{ 1024ULL*1024ULL*1024ULL*1024ULL, "TB" },
			};

		Unit kB[kB_NrUnits] = {
				{                            1ULL, "kB" },
				{                         1024ULL, "MB" },
				{                 1024ULL*1024ULL, "GB" },
				{         1024ULL*1024ULL*1024ULL, "TB" },
			};

		Unit Duration[Duration_NrUnits] = {
				{                                    1ULL, "hns"     },		// 100 nanoseconds, the Windows FILETIME resolution
				{                                   10ULL, "us"      },
				{                           1000ULL*10ULL, "ms"      },
				{                   1000ULL*1000ULL*10ULL, "s"       },
				{             60ULL*1000ULL*1000ULL*10ULL, "min"     },
				{       60ULL*60ULL*1000ULL*1000ULL*10ULL, "h"       },
				{ 24ULL*60ULL*60ULL*1000ULL*1000ULL*10ULL, "day(s)"  },
			};

	}



	// ValueInUnits

	ValueInUnits ValueInUnits::Make(uint64 v, Slice<Units::Unit> units, Units::Unit const*& largestFitUnit)
	{
		// Find largest fit unit, if any
		largestFitUnit = nullptr;

		Units::Unit const* nextLargerUnit {};
		for (Units::Unit const& unit : units)
		{
			if (unit.m_size > v)
			{
				if (!v && unit.m_size == 1)
					largestFitUnit = &unit;
				else
					nextLargerUnit = &unit;

				break;
			}
			
			largestFitUnit = &unit;
		}

		if (!largestFitUnit || 1 == largestFitUnit->m_size)
		{
			// Base unit: no rounding, no decimal point
			return ValueInUnits(v);
		}

		// Non-base unit: use rounding, maybe use decimal point
		uint64 whole, mfrac;
		uint fracWidth, mfracDiv;

		while (true)
		{
			whole = (v / largestFitUnit->m_size);
			mfrac = ((v % largestFitUnit->m_size) * 1000) / largestFitUnit->m_size;

				 if (whole >= 100) { fracWidth = 0; mfrac += 500; mfracDiv =    0; }
			else if (whole >=  10) { fracWidth = 1; mfrac +=  50; mfracDiv =  100; }
			else                   { fracWidth = 2; mfrac +=   5; mfracDiv =   10; }

			if (mfrac >= 1000)
			{
				++whole;
				mfrac -= 1000;
			}

			if (!nextLargerUnit) break;

			uint64 nextUnits = (nextLargerUnit->m_size / largestFitUnit->m_size);
			if (whole < nextUnits) break;
					
			largestFitUnit = nextLargerUnit;
			nextLargerUnit = nullptr;
		}

			 if (1 == fracWidth) { if (whole >= 100) { fracWidth = 0; mfracDiv =   0; } }
		else if (2 == fracWidth) { if (whole >=  10) { fracWidth = 1; mfracDiv = 100; } }

		if (!fracWidth)
			return ValueInUnits(whole);

		return ValueInUnits(whole, (uint) (mfrac / mfracDiv), fracWidth);
	}

}
