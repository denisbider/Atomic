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

	}
}
