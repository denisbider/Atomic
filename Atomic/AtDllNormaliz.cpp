#include "AtIncludes.h"
#include "AtDllNormaliz.h"

#include "AtDllGetFunc.h"
#include "AtInitOnFirstUse.h"
#include "AtSpinLock.h"

#pragma warning (push)
#pragma warning (disable: 4073)		// L3: initializers put in library initialization area
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	namespace Internal
	{
		
		LONG volatile a_normaliz_initFlag {};
		HMODULE a_normaliz_h {};

	}

	using namespace Internal;


	HMODULE GetDll_normaliz()
	{
		InitOnFirstUse(&a_normaliz_initFlag,
			[] { a_normaliz_h = LoadLibraryExW(L"normaliz.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); } );

		return a_normaliz_h;
	}


	#pragma warning (disable: 4191)	// 'type cast': unsafe conversion from 'FARPROC' to '...'
	ATDLL_GETFUNC_IMPL(normaliz, NormalizeString)

	int Call_NormalizeString(NORM_FORM a, LPCWSTR b, int c, LPWSTR d, int e)
	{
		FuncType_NormalizeString fn = GetFunc_NormalizeString();
		if (fn) return fn(a, b, c, d, e);
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return 0;
	}

}
