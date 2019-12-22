// Implementation provided by Andrej Andolsek, in capacity as a developer for Bitvise Limited, on December 22, 2019.
//
// Based on the Intellectual Property License Option Agreement between Bitvise Limited and denis bider (December 2017),
// this is a Bitvise Derived Work which Bitvise Limited shared and denis bider accepted for inclusion in New Versions:
//
// "If Bitvise shares a Bitvise Derived Work with Licensor for inclusion in New Versions, and if Licensor accepts the
// Bitvise Derived Work for this purpose, then the full rights, title, and interest in and to this Bitvise Derived Work
// are automatically and permanently transferred to Licensor, and become included in the Licensed Parts from which the
// Bitvise Derived Work was derived, and become covered by the respective License."
//
// This notice is informative and documentative. Its presence is not required for the rights transfer to take effect.

#pragma once

#include "AtNum.h"


namespace At
{

	class WinVer
	{
	public:
		WinVer() = default; // Creates a currently running pseudo version
		WinVer(uint32 maj, uint32  min) : m_flags(VerSet), m_maj(maj), m_min(min) {}
		WinVer& SetSp(uint16  sp)       { m_flags |= SpSet;    m_sp = sp;       return *this; }
		WinVer& SetBuild(uint32  build) { m_flags |= BuildSet; m_build = build; return *this; }

		bool operator== (WinVer const& x) const;
		bool operator>= (WinVer const& x) const;
		bool operator<= (WinVer const& x) const { return x.operator>= (*this); }
		bool operator!= (WinVer const& x) const { return !operator== (x); }
		bool operator>  (WinVer const& x) const { return !operator<= (x); }
		bool operator<  (WinVer const& x) const { return !operator>= (x); }

		enum Flags { VerSet = 1, SpSet = 2, BuildSet = 4 };
		uint32 m_flags {};
		uint32 m_maj   {};
		uint32 m_min   {};
		uint16 m_sp    {};
		uint32 m_build {};
	};


	inline WinVer Win2000()				{ return WinVer(5, 0); }
	inline WinVer Win2000(uint16 sp)	{ return WinVer(5, 0).SetSp(sp); }
	inline WinVer WinXp()				{ return WinVer(5, 1); }
	inline WinVer WinXp(uint16 sp)		{ return WinVer(5, 1).SetSp(sp); }
	inline WinVer Win2003()				{ return WinVer(5, 2); }	// AKA Win2003R2
	inline WinVer Win2003(uint16 sp)	{ return WinVer(5, 2).SetSp(sp); }
	inline WinVer WinVista()			{ return WinVer(6, 0); }	// AKA Win2008
	inline WinVer WinVista(uint16 sp)	{ return WinVer(6, 0).SetSp(sp); }
	inline WinVer Win7()				{ return WinVer(6, 1); }	// AKA Win2008R2
	inline WinVer Win7(uint16 sp)		{ return WinVer(6, 1).SetSp(sp); }
	inline WinVer Win8()				{ return WinVer(6, 2); }	// AKA Win20012
	inline WinVer Win8(uint16 sp)		{ return WinVer(6, 2).SetSp(sp); }
	inline WinVer Win81()				{ return WinVer(6, 3); }	// AKA Win20012R2
	inline WinVer Win81(uint16 sp)		{ return WinVer(6, 3).SetSp(sp); }
	inline WinVer Win10()				{ return WinVer(10, 0); }
	inline WinVer Win10(uint32 build)	{ return WinVer(10, 0).SetBuild(build); }

	enum Win10Build : uint32 {
		v1507 = 10240,	// Threshold 1
		v1511 = 10586,	// Threshold 2	// November update
		v1607 =	14393,	// Redstone 1	// Anniversary Update		// AKA Win2016
		v1703 =	15063,	// Redstone 2	// Creators update
		v1709 = 16299,	// Redstone 3	// Fall Creators update	
		v1803 =	17134,	// Redstone 4	// April 2018 Update	
		v1809 = 17763,	// Redstone 5	// October 2018 Update		// AKA Win2019
		v1903 = 18362,	// 19H1			// May 2019 Update
		v1909 = 18363,	// 19H2			// November 2019 Update
	};

}
