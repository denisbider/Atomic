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

#include "AtIncludes.h"
#include "AtWinVer.h"

#include "AtDllNtDll.h"


namespace At
{
	namespace
	{

		bool WinVer_CompareWith(WinVer const& winVer, BYTE condition)
		{
			RTL_OSVERSIONINFOEXW ovi {};
			ovi.dwOSVersionInfoSize = sizeof(ovi);
			ovi.dwMajorVersion = winVer.m_maj;
			ovi.dwMinorVersion = winVer.m_min;
			if (winVer.m_flags & WinVer::SpSet)
				ovi.wServicePackMajor = winVer.m_sp;
			if (winVer.m_flags & WinVer::BuildSet)
				ovi.dwBuildNumber = winVer.m_build;
			
			ULONG typeMask = VER_MAJORVERSION | VER_MINORVERSION;
		
			ULONGLONG conditionMask {};
			conditionMask = VerSetConditionMask(conditionMask, VER_MAJORVERSION, condition);
			conditionMask = VerSetConditionMask(conditionMask, VER_MINORVERSION, condition);

			if (winVer.m_flags & WinVer::SpSet)
			{
				typeMask |= VER_SERVICEPACKMAJOR;	
				conditionMask = VerSetConditionMask(conditionMask, VER_SERVICEPACKMAJOR, condition);
			}

			if (winVer.m_flags & WinVer::BuildSet)
			{
				typeMask |= VER_BUILDNUMBER;	
				conditionMask = VerSetConditionMask(conditionMask, VER_BUILDNUMBER, condition);
			}
				
			return Call_RtlVerifyVersionInfo(&ovi, typeMask, conditionMask) == STATUS_SUCCESS;
		}

	} // anon


	bool WinVer::operator== (WinVer const& x) const
	{
		if (!((m_flags | x.m_flags) & VerSet))
			return true;
		else
		{
			unsigned int const commonFlags { m_flags & x.m_flags };
			if (!(commonFlags & VerSet))
			{
				if (m_flags & VerSet)
					return WinVer_CompareWith(*this, VER_EQUAL);
				else
					return WinVer_CompareWith(x, VER_EQUAL);
			}
			else
			{
				if (m_maj != x.m_maj)
					return false;
				if (m_min != x.m_min)
					return false;
			
				if (commonFlags & SpSet)
					if (m_sp != x.m_sp)
						return false;

				if (commonFlags & BuildSet)
					if (m_build != x.m_build)
						return false;

				return true;
			}
		}
	}


	bool WinVer::operator>= (WinVer const& x) const
	{
		if (!((m_flags | x.m_flags) & VerSet))
			return true;
		else
		{
			unsigned int const commonFlags { m_flags & x.m_flags };
			if (!(commonFlags & VerSet))
			{
				if (m_flags & VerSet)
					return WinVer_CompareWith(*this, VER_LESS_EQUAL);
				else
					return WinVer_CompareWith(x, VER_GREATER_EQUAL);
			}
			else
			{
				if (m_maj > x.m_maj)
					return true;
				if (m_maj < x.m_maj)
					return false;

				if (m_min > x.m_min)
					return true;
				if (m_min < x.m_min)
					return false;

				if (commonFlags & SpSet)
				{
					if (m_sp > x.m_sp)
						return true;
					if (m_sp < x.m_sp)
						return false;
				}
			
				if (commonFlags & BuildSet)
				{
					if (m_build > x.m_build)
						return true;
					if (m_build < x.m_build)
						return false;
				}
						
				return true;
			}
		}
	}

}
