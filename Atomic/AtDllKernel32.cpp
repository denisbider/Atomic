#include "AtIncludes.h"
#include "AtDllKernel32.h"

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
		
		LONG volatile a_kernel32_initFlag {};
		HMODULE a_kernel32_h {};

	}

	using namespace Internal;


	HMODULE GetDll_kernel32()
	{
		InitOnFirstUse(&a_kernel32_initFlag,
			[] { a_kernel32_h = GetModuleHandleW(L"kernel32.dll"); } );

		return a_kernel32_h;
	}


	#pragma warning (disable: 4191)	// 'type cast': unsafe conversion from 'FARPROC' to '...'
	ATDLL_GETFUNC_IMPL(kernel32, CancelIoEx)
	ATDLL_GETFUNC_IMPL(kernel32, GetSystemTimePreciseAsFileTime)
	ATDLL_GETFUNC_IMPL(kernel32, GetTickCount64)



	// CancelIoEx_or_CancelIo

	BOOL CancelIoEx_or_CancelIo(HANDLE h, LPOVERLAPPED ovl)
	{
		FuncType_CancelIoEx cancelIoEx = GetFunc_CancelIoEx();
		if (cancelIoEx)
			return cancelIoEx(h, ovl);

		return CancelIo(h);
	}



	// E_GetTickCount64

	namespace
	{
		SpinLock a_spinLock_tickCount;
		DWORD    a_lastTickCount {};
		DWORD    a_tickCountHi   {};
	}

	ULONGLONG E_GetTickCount64()
	{
		FuncType_GetTickCount64 getTickCount64 = GetFunc_GetTickCount64();
		if (getTickCount64)
			return getTickCount64();

		a_spinLock_tickCount.Acquire();
		OnExit releaseLock = [] { a_spinLock_tickCount.Release(); };

		DWORD tickCount = GetTickCount();
		if (a_lastTickCount <= tickCount)
			a_lastTickCount = tickCount;
		else if (a_lastTickCount - tickCount > 60 * 1000)	// Allow for GetTickCount() to backtrack, but not by more than a minute
			++a_tickCountHi;

		return (((ULONGLONG) a_tickCountHi) << 32) | (ULONGLONG) tickCount;
	}

}
