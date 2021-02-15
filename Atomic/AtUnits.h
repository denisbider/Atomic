#pragma once

#include "AtSlice.h"


namespace At
{
	namespace Units
	{

		struct Unit
		{
			uint64      m_size;
			char const* m_zName;

			Unit(uint64 size, char const* zName) : m_size(size), m_zName(zName) {}
		};


		// Base unit first, each subsequent unit must increase in size

		enum { Bytes_NrUnits = 5 };
		extern Unit Bytes[Bytes_NrUnits];

		enum { kB_NrUnits = 4 };
		extern Unit kB[kB_NrUnits];

		enum { Duration_NrUnits = 7 };
		extern Unit Duration[Duration_NrUnits];
				
	}


	// ValueInUnits

	struct ValueInUnits
	{
		uint64 m_whole     {};
		uint   m_frac      {};
		uint   m_fracWidth {};

		static ValueInUnits Make(uint64 v, Slice<Units::Unit> units, Units::Unit const*& largestFitUnit);

	private:
		ValueInUnits(uint64 whole, uint frac = 0, uint fracWidth = 0) : m_whole(whole), m_frac(frac), m_fracWidth(fracWidth) {}
	};

}
