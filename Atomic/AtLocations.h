#pragma once

#include "AtBulkAlloc.h"
#include "AtCsv.h"
#include "AtMutex.h"
#include "AtRp.h"
#include "AtSocket.h"
#include "AtTime.h"
#include "AtThread.h"
#include "AtVec.h"


namespace At
{

	// Requires files "IpBlocks.csv" and "Locations.csv" in "Locations" subdirectory under application's main module directory.
	// Files must be in MaxMind GeoLite/GeoIP City format, stored in the Windows-1252 code page.
	// Ignores the first two lines of each file; assumes they are copyright line and headers line.
	// The Locations file is parsed to initialize an in-memory locations database.
	// The IpBlocks file is translated into a memory-mapped file containing the same information in condensed form.
	// Both input files are closed after loading, allowing external applications to modify the CSV files.
	// To initialize Locations, start it as a thread. Locations is initialized in BeforeThreadStart().
	// After first loading, the thread will monitor the Locations subdirectory for a file named "ReInit".
	// If this file appears, information from both CSV files will be reloaded, and the ReInit file deleted.
	
	struct TLCode
	{
		TLCode()      { code[0] = 0; code[1] = 0; }
		TLCode(Seq s) { Set(s); }
		
		byte code[2];
		
		bool Any() const { return code[0] != ' ' || code[1] != ' '; }
		Seq Code() const { if (code[1] != ' ') return Seq(code, 2); if (code[0] != ' ') return Seq(code, 1); return Seq(); }
		void Set(Seq s);

		bool operator<  (TLCode const& x) const { return code[0] < x.code[0] || (code[0] == x.code[0] && code[1] < x.code[1]); }
		bool operator== (TLCode const& x) const { return code[0] == x.code[0] && code[1] == x.code[1]; }
	};
	
	struct LocInfo
	{
		LocInfo() : latitude(0.0), longitude(0.0) {}
		LocInfo(LocInfo&& x) : name(std::move(x.name)), tlCountryCode(x.tlCountryCode), latitude(x.latitude), longitude(x.longitude) {}
		LocInfo& operator= (LocInfo x) { name.Swap(x.name); std::swap(tlCountryCode, x.tlCountryCode); std::swap(latitude, x.latitude); std::swap(longitude, x.longitude); return *this; }

		void Clear() { name.Clear(); tlCountryCode.Set(Seq()); latitude = 0.0; longitude = 0.0; }
		
		Str name;
		TLCode tlCountryCode;
		double latitude;
		double longitude;
	};

	class Locations : public Thread
	{
	public:
		// If the first initialization fails, there will be an exception when trying to start the Locations thread.
		// If a subsequent reload fails, the previously loaded state will be kept, so Locations can continue to be used.
		// These methods can be used to discover the success or failure of subsequent reloads.

		Time StateLoadedTime   () const { Locker locker(m_mx); return m_stateLoadedTime; }
		Seq  LastStateLoadError() const { Locker locker(m_mx); return m_lastStateLoadError; }
	
		// Main usage interface.
		// Must initialize Locations by starting its thread before using below methods.
		// Methods are thread safe.
	
		struct CityMatch
		{
			Str cityKey;
			Str locName;
		};

		void GetCountries(Vec<TLCode>& withPostalCodes, Vec<TLCode>& noPostalCodes);
		bool LookupIpAddress(Seq ipStr, LocInfo& locInfo) const;
		bool LookupIpAddress(SockAddr const& sa, LocInfo& locInfo) const;
		bool LookupPostalCode(Seq countryCode, Seq postalCode, LocInfo& locInfo) const;
		bool LookupCityKey(Seq cityKey, LocInfo& locInfo) const;
		void LookupPartialCityName(Seq countryCode, Seq partialCityName, Vec<CityMatch>& topMatches, sizet matchesToReturn) const;
	
	private:
		void BeforeThreadStart();
		void ThreadMain();
	
		Str m_locationsDir;
		Str m_reInitFilePath;
		Str m_ipBlocksFilePath;
		Str m_locationsFilePath;
	
		struct Country
		{
			TLCode tlCountryCode;
			sizet definitions;
			sizet definitionsWithPostalCodes;
		};

		enum { MaxCityNameLen = 27 };
	
		struct City
		{
			TLCode tlCountryCode;
			TLCode tlRegionCode;
			byte cityNameLen;
			byte arCityName[MaxCityNameLen];
			double sumLatitudes;
			double sumLongitudes;
			sizet definitions;
			sizet references;
		
			Seq CityName() const  { return Seq(arCityName, (sizet) cityNameLen); }
			double Latitude() const  { return sumLatitudes  / definitions; }
			double Longitude() const { return sumLongitudes / definitions; }
		};
	
		enum { MaxPostalCodeLen = 9 };
	
		struct PostalCode
		{
			City* city;
			double sumLatitudes;
			double sumLongitudes;
			sizet definitions;
			byte postalCodeLen;
			byte arPostalCode[MaxPostalCodeLen];

			double Latitude() const   { return sumLatitudes  / definitions; }
			double Longitude() const  { return sumLongitudes / definitions; }
		};
	
		struct Location
		{
			City* city;
			PostalCode* postalCode;
			double latitude;
			double longitude;
		};
	
		typedef std::map     <Str, Country,    std::less<Str>, BulkAlloc<std::pair<Str, Country>>> Countries;       // Key: country code
		typedef std::map     <Str, City,       std::less<Str>, BulkAlloc<std::pair<Str, Country>>> Cities;          // Key: country code + region code + lower(city name)
		typedef std::multimap<Str, City*,      std::less<Str>, BulkAlloc<std::pair<Str, Country>>> CitiesByName;    // Key: country code + lower(city name)
		typedef std::map     <Str, PostalCode, std::less<Str>, BulkAlloc<std::pair<Str, Country>>> PostalCodes;     // Key: country code + postal code

		struct IpBlock
		{
			uint32 locId;
			uint32 ipFrom;
			uint32 ipTo;
		};

		struct State : public RefCountable, public BulkStorage
		{
			State();
			~State();

			Countries countries;
			Cities cities;
			CitiesByName citiesByName;
			PostalCodes postalCodes;
			Vec<Location> locations;
		
			Vec<TLCode> countriesWithPostalCodes;		// Ordered by number of references
			Vec<TLCode> countriesNoPostalCodes;			// Ordered by country name alphabetically
		
			sizet    mappingSize {};
			HANDLE   hMapping    {};
			IpBlock* pView       {};
			sizet    nrIpBlocks  {};
		};
	
		mutable Mutex m_mx;
		Rp<State> m_state;
		Str m_lastStateLoadError;				// empty if no error in last state load attempt
		Time m_stateLoadedTime;					// Not set if not yet successfully loaded
	
		struct CsvLocation
		{
			sizet id;
			Str countryCode;
			Str regionCode;
			Str cityName;
			Str postalCode;
			double latitude;
			double longitude;
		};

		void GetState(Rp<State>& state) const;
		bool MakeLocName(Str& locName, Seq countryCode, Seq regionCode, Seq cityName) const;
	
		void ReInit();
		void LoadNewState(State& state);

		void ProcessLocationsFile(State& state, CsvReader& reader, sizet csvSize);
		void AddCountryDefinition(State& state, CsvLocation& loc);
		City* AddCityDefinition(State& state, CsvLocation& loc);
		PostalCode* AddPostalCodeDefinition(State& state, CsvLocation& loc, City* city);
		void AddLocationDefinition(State& state, CsvLocation& loc, City* city, PostalCode* postalCode);
		void SortCountries(State& state);

		static Str MakeCityKey(Seq countryCode, Seq regionCode, Seq cityName);

		void ProcessIpBlocksFile(State& state, CsvReader& reader, sizet csvSize);

	private:	
		static void SetStr(byte& len, byte* dest, byte maxLen, Seq s);

		static void SetCityName  (byte& len, byte* dest, Seq s) { SetStr(len, dest, MaxCityNameLen,   s); }
		static void SetPostalCode(byte& len, byte* dest, Seq s) { SetStr(len, dest, MaxPostalCodeLen, s); }
	};

}
