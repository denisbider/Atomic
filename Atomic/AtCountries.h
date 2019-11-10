#pragma once

#include "AtSeq.h"

namespace At
{
	// Looks up country name using provided ISO 3166-1 alpha-2 country code.
	// Recognizes officially assigned and exceptionally/transitionally/indeterminately reserved codes as of August 2011.

	Seq CountryName_From_Iso3166_1_Alpha2(Seq code);



	// Looks up region name based on country code (must be 2 letter ISO 3166-1 alpha-2) and region code (country-specific).
	// Currently implemented for US and Canada.

	inline bool IsCountryWithRegionDivision(Seq countryCode)
		{ return countryCode == "US" || countryCode == "CA"; }

	Seq RegionName_US(Seq code);
	Seq RegionName_Canada(Seq code);

	Seq CountryRegionName(Seq countryCode, Seq regionCode);
}
