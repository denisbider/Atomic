#pragma once

#include "AtWinErr.h"


namespace At
{

	DWORD WaitHandles(DWORD count, HANDLE* handles, DWORD milliseconds);

	inline DWORD Wait3(HANDLE h1, HANDLE h2, HANDLE h3, DWORD ms) { HANDLE hs[3] = { h1, h2, h3 }; return WaitHandles(3, hs, ms); }
	inline DWORD Wait2(HANDLE h1, HANDLE h2,            DWORD ms) { HANDLE hs[2] = { h1, h2 };     return WaitHandles(2, hs, ms); }
	inline DWORD Wait1(HANDLE h,                        DWORD ms) {                                return WaitHandles(1, &h, ms); }

}
