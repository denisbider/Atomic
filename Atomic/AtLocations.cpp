#include "AtIncludes.h"
#include "AtLocations.h"

#include "AtCountries.h"
#include "AtFile.h"
#include "AtNumCvt.h"
#include "AtPath.h"
#include "AtSocket.h"
#include "AtUtf8.h"
#include "AtWait.h"
#include "AtWinStr.h"


namespace At
{

	void TLCode::Set(Seq s)
	{
		code[0] = If(s.n > 0, byte, s.p[0], ' ');
		code[1] = If(s.n > 1, byte, s.p[1], ' ');
	}


	void Locations::GetCountries(Vec<TLCode>& withPostalCodes, Vec<TLCode>& noPostalCodes)
	{
		Rp<State> state;
		GetState(state);

		withPostalCodes = state->countriesWithPostalCodes;
		noPostalCodes = state->countriesNoPostalCodes;
	}


	bool Locations::LookupIpAddress(Seq ipStr, LocInfo& locInfo) const
	{
		SockAddr sa;
		if (!sa.Parse(ipStr).Valid())
			return false;
	
		return LookupIpAddress(sa, locInfo);	
	}


	bool Locations::LookupIpAddress(SockAddr const& sa, LocInfo& locInfo) const
	{
		locInfo.Clear();

		Rp<State> state;
		GetState(state);
	
		if (!sa.IsIp4())
			return false;
		uint32 ipNr = sa.GetIp4Nr();

		// Find IpBlock containing this IP
		IpBlock* first = state->pView;
		IpBlock* last = state->pView + (state->nrIpBlocks - 1);
		uint32 locId = 0;
		while (true)
		{
			if (ipNr < first->ipFrom || ipNr > last->ipTo)
				return false;
	
			if ((ipNr >= first->ipFrom) && (ipNr <= first->ipTo))
				{ locId = first->locId; break; }
		
			if ((ipNr >= last->ipFrom) && (ipNr <= last->ipTo))
				{ locId = last->locId; break; }
		
			sizet dist = (sizet) (last - first);
			if (dist <= 1)
				return false;
		
			IpBlock* mid = first + (dist/2);
			if (ipNr < mid->ipFrom)
				last = mid;
			else
				first = mid;
		}
	
		// Found IpBlock, look up Location
		Location& loc = state->locations[locId];
		if (!MakeLocName(locInfo.name, loc.city->tlCountryCode.Code(), loc.city->tlRegionCode.Code(), loc.city->CityName()))
			return false;
	
		locInfo.tlCountryCode = loc.city->tlCountryCode;
		locInfo.latitude = loc.latitude;
		locInfo.longitude = loc.longitude;
		return true;
	}


	bool Locations::LookupPostalCode(Seq countryCodeArg, Seq postalCodeArg, LocInfo& locInfo) const
	{
		locInfo.Clear();

		Rp<State> state;
		GetState(state);

		Str countryCode, postalCode;
		countryCode.Upper(countryCodeArg);
		postalCode.Upper(postalCodeArg);
	
		Str key;
		key.ReserveExact(countryCode.Len() + postalCode.Len())
		   .Add(countryCode)
		   .Add(postalCode);
	
		PostalCodes::const_iterator it = state->postalCodes.find(key);
		if (it == state->postalCodes.end())
			return false;
	
		if (!MakeLocName(locInfo.name, it->second.city->tlCountryCode.Code(), it->second.city->tlRegionCode.Code(), it->second.city->CityName()))
			return false;
	
		locInfo.tlCountryCode = it->second.city->tlCountryCode;
		locInfo.latitude = it->second.Latitude();
		locInfo.longitude = it->second.Longitude();
		return true;
	}


	bool Locations::LookupCityKey(Seq cityKey, LocInfo& locInfo) const
	{
		locInfo.Clear();

		Rp<State> state;
		GetState(state);

		Cities::const_iterator it = state->cities.find(cityKey);
		if (it == state->cities.end())
			return false;
	
		if (!MakeLocName(locInfo.name, it->second.tlCountryCode.Code(), it->second.tlRegionCode.Code(), it->second.CityName()))
			return false;
	
		locInfo.tlCountryCode = it->second.tlCountryCode;
		locInfo.latitude = it->second.Latitude();
		locInfo.longitude = it->second.Longitude();
		return true;
	}


	void Locations::LookupPartialCityName(
		Seq countryCodeArg, Seq partialCityNameArg, Vec<CityMatch>& topMatches, sizet matchesToReturn) const
	{
		topMatches.Clear();

		Rp<State> state;
		GetState(state);
	
		Str countryCode = ToUpper(countryCodeArg);
		Str partialCityName = ToLower(partialCityNameArg);

		Str cbnKey;
		cbnKey.ReserveExact(countryCode.Len() + partialCityName.Len())
			  .Add(countryCode)
			  .Add(partialCityName);
	
		typedef std::multimap<sizet, CityMatch> Matches;
		Matches matches;
	
		CitiesByName::const_iterator cbnIt = state->citiesByName.lower_bound(cbnKey);
		for (; cbnIt != state->citiesByName.end() && Seq(cbnIt->first).StartsWithExact(cbnKey); ++cbnIt)
		{
			City* city = cbnIt->second;
			CityMatch match;
			match.cityKey = MakeCityKey(city->tlCountryCode.Code(), city->tlRegionCode.Code(), city->CityName());		
			if (MakeLocName(match.locName, city->tlCountryCode.Code(), city->tlRegionCode.Code(), city->CityName()))
				matches.insert(std::make_pair(city->references, match));
		}

		Matches::const_reverse_iterator mit = matches.rbegin();
		for (; mit != matches.rend(); ++mit)
		{
			topMatches.Add(mit->second);
			if (topMatches.Len() >= matchesToReturn)
				break;
		}
	}


	 void Locations::GetState(Rp<State>& state) const
	{
		Locker locker(m_mx);
		state = m_state;
		EnsureThrow(state.Any());
	}


	bool Locations::MakeLocName(Str& locName, Seq countryCode, Seq regionCode, Seq cityName) const
	{
		locName.Clear();
		if (!cityName.n)
			return false;

		if (countryCode == "GB")
			countryCode = "UK";

		if (IsCountryWithRegionDivision(countryCode) && regionCode.n)
		{
			locName.ReserveExact(cityName.size() + 8)
				   .Add(cityName).Add(", ").Add(regionCode).Add(", ").Add(countryCode);
		}
		else
		{
			locName.ReserveExact(cityName.size() + 4)
				   .Add(cityName).Add(", ").Add(countryCode);
		}
		return true;
	}


	void Locations::BeforeThreadStart()
	{
		m_locationsDir = JoinPath(GetDirectoryOfFileName(GetModulePath()), "Locations");
		m_reInitFilePath = JoinPath(m_locationsDir, "ReInit");
		m_ipBlocksFilePath = JoinPath(m_locationsDir, "IpBlocks.csv");
		m_locationsFilePath = JoinPath(m_locationsDir, "Locations.csv");

		m_state = new State();
		LoadNewState(m_state.Ref());
		m_stateLoadedTime = Time::StrictNow();
	}


	void Locations::ThreadMain()
	{
		// This code can be disabled so that the thread terminates immediately,
		// so that the application releases the last reference to Locations when it exits,
		// so that Locations cleanup occurs on application exit, so we can see how long it takes.
	#if 1
		HANDLE hcn = FindFirstChangeNotification(WinStr(m_locationsDir).Z(), false, FILE_NOTIFY_CHANGE_FILE_NAME);
		if (hcn == INVALID_HANDLE_VALUE)
		{
			DWORD rc = GetLastError();
			throw WinErr<>(rc, Str("Error in FindFirstChangeNotification for ").Add(m_locationsDir));
		}
	
		OnExit closeChangeNotification( [&] () { FindCloseChangeNotification(hcn); } );
	
		while (true)
		{
			Wait1(hcn, INFINITE);
		
			DWORD reInitAttrs = GetFileAttributes(WinStr(m_reInitFilePath).Z());
			if (reInitAttrs != INVALID_FILE_ATTRIBUTES && ((reInitAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0))
				ReInit();
		
			if (!FindNextChangeNotification(hcn))
			{
				DWORD rc = GetLastError();
				throw WinErr<>(rc, Str("Error in FindNextChangeNotification for ").Add(m_locationsDir));
			}
		}
	#endif
	}


	void Locations::ReInit()
	{
		Rp<State> newState(new State());

		try
		{
			LoadNewState(newState.Ref());
		}
		catch (Exception const& e)
		{
			Locker locker(m_mx);
			m_lastStateLoadError = e.what();
			return;
		}
	
		Locker locker(m_mx);
		m_stateLoadedTime = Time::StrictNow();
		m_state = newState;
	}


	Locations::State::State()
		: countries   (Countries   ::key_compare(), Countries   ::allocator_type(BulkStorage::Self()))
		, cities      (Cities      ::key_compare(), Cities      ::allocator_type(BulkStorage::Self()))
		, citiesByName(CitiesByName::key_compare(), CitiesByName::allocator_type(BulkStorage::Self()))
		, postalCodes (PostalCodes ::key_compare(), PostalCodes ::allocator_type(BulkStorage::Self()))
	{
	}


	Locations::State::~State()
	{
		if (pView != nullptr)
			UnmapViewOfFile(pView);

		if (hMapping != 0)
			CloseHandle(hMapping);
	}


	void Locations::LoadNewState(State& state)
	{
		// Load Locations and IpBlocks files
		FileLoader locationsLoader { m_locationsFilePath };
		FileLoader ipBlocksLoader  { m_ipBlocksFilePath  };
		CsvReader  locationsReader { locationsLoader, ',', 0 };
		CsvReader  ipBlocksReader  { ipBlocksLoader,  ',', 0 };
		locationsReader.SkipLines(2);
		ipBlocksReader .SkipLines(2);

		// Delete ReInit file if it exists
		DeleteFileW(WinStr(m_reInitFilePath).Z());

		// Process Locations and IpBlocks files
		ProcessLocationsFile (state, locationsReader, locationsLoader.Content().n );
		ProcessIpBlocksFile  (state, ipBlocksReader,  ipBlocksLoader .Content().n );
	}


	void Locations::ProcessLocationsFile(State& state, CsvReader& reader, sizet csvSize)
	{
		// Smallest CSV-encoded location in GeoLite City is 32 bytes
		uint64 maxNrLocations = csvSize / 32;
		if (maxNrLocations > SIZE_MAX)
			throw InputErr(Str("Input file ").Add(reader.Path()).Add(" is too large"));

		state.locations.ResizeExact(NumCast<sizet>(maxNrLocations));

		sizet recordNr = 0;
		Vec<wchar_t> convertBuf;
		CsvLocation loc;
		Vec<Str> record;
		sizet nrFields;
		record.ResizeExact(7);
		while (reader.ReadRecord(record, nrFields))
		{
			++recordNr;
		
			Seq locIdStr(record[0]);
			uint64 locId64 = locIdStr.ReadNrUInt64Dec();
			if (locIdStr.n)
				throw InputErr(Str("Invalid locId at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
			if (locId64 > SIZE_MAX)
				throw InputErr(Str("locId out of bounds at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));

			loc.id = NumCast<sizet>(locId64);

			loc.countryCode.Clear();
			if (record[1].Len() == 2)
				loc.countryCode.Upper(record[1]);
		
			loc.regionCode.Clear();
			if (record[2].Len() == 2)
				loc.regionCode.Upper(record[2]);

			StrCvtCp(record[3], loc.cityName, 1252, CP_UTF8, convertBuf);
			loc.postalCode.Clear().Upper(record[4]);

			Seq latitudeStr(record[5]);
			loc.latitude = latitudeStr.ReadDouble();
			if (latitudeStr.n)
				throw InputErr(Str("Invalid latitude at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
		
			Seq longitudeStr(record[6]);
			loc.longitude = longitudeStr.ReadDouble();
			if (longitudeStr.n)
				throw InputErr(Str("Invalid longitude at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
		
			City* city = nullptr;
			PostalCode* postalCode = nullptr;
		
			if (loc.countryCode.Any())
			{
				AddCountryDefinition(state, loc);
		
				if (loc.cityName.Any())
				{
					city = AddCityDefinition(state, loc);

					if (loc.postalCode.Any())
						postalCode = AddPostalCodeDefinition(state, loc, city);
				}
			}

			AddLocationDefinition(state, loc, city, postalCode);
		}
	
		SortCountries(state);
	}


	void Locations::AddCountryDefinition(State& state, CsvLocation& loc)
	{
		Countries::iterator it = state.countries.find(loc.countryCode);
		if (it != state.countries.end())
		{
			++(it->second.definitions);
			if (loc.postalCode.Any())
				++(it->second.definitionsWithPostalCodes);
		}
		else
		{
			it = state.countries.insert(std::make_pair(loc.countryCode, Country())).first;
			it->second.tlCountryCode.Set(loc.countryCode);
			it->second.definitions = 1;
			it->second.definitionsWithPostalCodes = (sizet) (!loc.postalCode.Any() ? 0 : 1);
		}
	}


	Str Locations::MakeCityKey(Seq countryCode, Seq regionCode, Seq cityName)
	{
		Str key;
		key.ReserveExact(4 + cityName.n)
		   .Add(countryCode).Add(regionCode)
		   .Lower(Utf8::TruncateStr(cityName, MaxCityNameLen));
		return key;
	}


	Locations::City* Locations::AddCityDefinition(State& state, CsvLocation& loc)
	{
		Str key(MakeCityKey(loc.countryCode, loc.regionCode, loc.cityName));	
		Cities::iterator it = state.cities.find(key);
		if (it != state.cities.end())
		{
			it->second.sumLatitudes += loc.latitude;
			it->second.sumLongitudes += loc.longitude;
			++(it->second.definitions);
		}
		else
		{
			it = state.cities.insert(std::make_pair(key, City())).first;
			it->second.tlCountryCode.Set(loc.countryCode);
			it->second.tlRegionCode.Set(loc.regionCode);
			SetCityName(it->second.cityNameLen, it->second.arCityName, loc.cityName);
			it->second.sumLatitudes = loc.latitude;
			it->second.sumLongitudes = loc.longitude;
			it->second.definitions = 1;
			it->second.references = 1;
		
			Str cbnKey;
			cbnKey.ReserveExact(2 + (sizet) (it->second.cityNameLen))
				  .Add(loc.countryCode)
				  .Lower(it->second.CityName());
		
			state.citiesByName.insert(std::make_pair(cbnKey, &(it->second)));
		}
	
		return &(it->second);
	}


	Locations::PostalCode* Locations::AddPostalCodeDefinition(State& state, CsvLocation& loc, City* city)
	{
		Str key;
		key.ReserveExact(2 + loc.postalCode.Len())
		   .Add(loc.countryCode)
		   .Add(loc.postalCode);
	
		PostalCodes::iterator it = state.postalCodes.find(key);
		if (it != state.postalCodes.end())
		{
			it->second.sumLatitudes += loc.latitude;
			it->second.sumLongitudes += loc.longitude;
			++(it->second.definitions);
		}
		else
		{
			it = state.postalCodes.insert(std::make_pair(key, PostalCode())).first;
			it->second.city = city;
			SetPostalCode(it->second.postalCodeLen, it->second.arPostalCode, loc.postalCode);
			it->second.sumLatitudes = loc.latitude;
			it->second.sumLongitudes = loc.longitude;
			it->second.definitions = 1;
		}
	
		return &(it->second);
	}


	void Locations::AddLocationDefinition(State& state, CsvLocation& loc, City* city, PostalCode* postalCode)
	{
		Location& l = state.locations[loc.id];
		l.city = city;
		l.postalCode = postalCode;
		l.latitude = loc.latitude;
		l.longitude = loc.longitude;
	}


	void Locations::SortCountries(State& state)
	{
		std::multimap<sizet, TLCode> countriesWithPostalCodes;
		std::map<Seq, TLCode> countriesNoPostalCodes;
	
		{
			Countries::const_iterator it = state.countries.begin();
			for (; it != state.countries.end(); ++it)
				if (5 * it->second.definitionsWithPostalCodes > 4 * it->second.definitions)
					countriesWithPostalCodes.insert(std::make_pair(it->second.definitions, it->second.tlCountryCode));
				else
					countriesNoPostalCodes.insert(
						std::make_pair(
							CountryName_From_Iso3166_1_Alpha2(it->second.tlCountryCode.Code()),
							it->second.tlCountryCode));
		}

		{	
			state.countriesWithPostalCodes.ReserveExact(countriesWithPostalCodes.size());
			std::multimap<sizet, TLCode>::const_iterator it = countriesWithPostalCodes.begin();
			for (; it != countriesWithPostalCodes.end(); ++it)
				state.countriesWithPostalCodes.Add(it->second);
		}
	
		{
			state.countriesNoPostalCodes.ReserveExact(countriesNoPostalCodes.size());
			std::map<Seq, TLCode>::const_iterator it = countriesNoPostalCodes.begin();
			for (; it != countriesNoPostalCodes.end(); ++it)
				state.countriesNoPostalCodes.Add(it->second);
		}
	}


	void Locations::ProcessIpBlocksFile(State& state, CsvReader& reader, sizet csvSize)
	{
		// Initializes memory-mapped file
		// CSV-encoded LocId,IpFrom,IpTo triplet cannot be smaller than 20 bytes as long as IP >= 10.0.0.0
		uint64 mappingSize64 = (((uint64) csvSize) / 20) * sizeof(IpBlock);
		if (mappingSize64 > SIZE_MAX)
			throw ZLitErr("IpBlocks file too large");

		state.mappingSize = NumCast<sizet>(mappingSize64);

		DWORD mappingSizeHi = (DWORD) ((((uint64) state.mappingSize) >> 32) & 0xFFFFFFFF);
		DWORD mappingSizeLo = (DWORD) ((((uint64) state.mappingSize)      ) & 0xFFFFFFFF);

		state.hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, mappingSizeHi, mappingSizeLo, 0);
		if (!state.hMapping)
			{ LastWinErr e; throw e.Make<>("Error in CreateFileMapping for IpBlocks file"); }
	
		state.pView = (IpBlock*) MapViewOfFile(state.hMapping, FILE_MAP_WRITE, 0, 0, state.mappingSize);
		if (!state.pView)
			{ LastWinErr e; throw e.Make<>("Error in MapViewOfFile for IpBlocks file"); }

		// Translate records in IpBlocks file
		sizet recordNr = 0;
		Vec<Str> record;
		sizet nrFields;
		record.ResizeExact(3);
		while (reader.ReadRecord(record, nrFields))
		{
			IpBlock& ipBlock = state.pView[recordNr++];
			if (recordNr * sizeof(IpBlock) > state.mappingSize)
				throw StrErr(Str("IpBlock storage exceeded at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
		
			Seq ipFromStr = Seq(record[0]);
			ipBlock.ipFrom = ipFromStr.ReadNrUInt32();
			if (ipFromStr.n)
				throw InputErr(Str("Invalid ipFrom at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
			
			Seq ipToStr = Seq(record[1]);
			ipBlock.ipTo = ipToStr.ReadNrUInt32();
			if (ipToStr.n)
				throw InputErr(Str("Invalid ipTo at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
			if (ipBlock.ipTo < ipBlock.ipFrom)
				throw InputErr(Str("ipTo is less than ipFrom at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));

			Seq locIdStr = Seq(record[2]);
			ipBlock.locId = locIdStr.ReadNrUInt32();
			if (locIdStr.n)
				throw InputErr(Str("Invalid locId at record ").UInt(recordNr).Add(" in ").Add(reader.Path()));
		
			Location& loc = state.locations[ipBlock.locId];
			if (loc.city)
				++(loc.city->references);
		}
	
		state.nrIpBlocks = recordNr;
	}


	void Locations::SetStr(byte& len, byte* dest, byte maxLen, Seq s)
	{
		s = Utf8::TruncateStr(s, maxLen);
		len = (byte) s.n;
		memcpy(dest, s.p, len);
	}

}

