#include "AtIncludes.h"
#include "AtBhtpn.h"

#include "AtCrypt.h"
#include "AtPath.h"

namespace At
{
	Str Gen_ModulePathBased_BhtpnPipeName(Seq modulePath)
	{
		Str modulePathStr;
		if (!modulePath.n)
		{
			modulePathStr = GetModulePath();
			modulePath = modulePathStr;
		}

		Seq dirAndBase = PathParts(modulePath).m_dirAndBaseName;
		SeqLower dirAndBaseLower { dirAndBase };
		if (dirAndBaseLower.EndsWithExact("admin"))
			dirAndBaseLower.n -= 5;

		return Hash::HexHashOf(dirAndBaseLower, CALG_SHA_256, CharCase::Lower).Resize(16);
	}
}
