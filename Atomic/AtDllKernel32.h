#pragma once

#include "AtIncludes.h"


namespace At
{

	HMODULE GetDll_kernel32();

	typedef BOOL (__stdcall* FuncType_CancelIoEx)(HANDLE, LPOVERLAPPED);
	FuncType_CancelIoEx GetFunc_CancelIoEx();
	BOOL CancelIoEx_or_CancelIo(HANDLE h, LPOVERLAPPED ovl);

	typedef void (__stdcall* FuncType_GetSystemTimePreciseAsFileTime)(LPFILETIME);
	FuncType_GetSystemTimePreciseAsFileTime GetFunc_GetSystemTimePreciseAsFileTime();

	typedef ULONGLONG (__stdcall* FuncType_GetTickCount64)();
	FuncType_GetTickCount64 GetFunc_GetTickCount64();
	ULONGLONG E_GetTickCount64();

}
