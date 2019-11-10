#pragma once

#include "AtIncludes.h"


namespace At
{

	#define ATDLL_GETFUNC_IMPL(DLLNAME, NAME) \
		namespace { \
			LONG volatile a_func_##NAME##_flag {}; \
			FuncType_##NAME a_func_##NAME {}; \
		} \
		FuncType_##NAME GetFunc_##NAME() { \
			InitOnFirstUse(&a_func_##NAME##_flag, \
				[] { a_func_##NAME = (FuncType_##NAME) GetProcAddress(GetDll_##DLLNAME(), #NAME); } ); \
			return a_func_##NAME; \
		}

}
