#pragma once

#include "AtIncludes.h"


namespace At
{

	HMODULE GetDll_normaliz();

	typedef int (__stdcall* FuncType_NormalizeString)(NORM_FORM, LPCWSTR, int, LPWSTR, int);
	FuncType_NormalizeString GetFunc_NormalizeString();
	int Call_NormalizeString(NORM_FORM, LPCWSTR, int, LPWSTR, int);

}
