#include "AtIncludes.h"
#include "AtDllNtDll.h"

#include "AtDllGetFunc.h"
#include "AtException.h"
#include "AtInitOnFirstUse.h"

#pragma warning (push)
#pragma warning (disable: 4073)		// L3: initializers put in library initialization area
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	namespace Internal
	{
		
		LONG volatile a_ntdll_initFlag {};
		HMODULE a_ntdll_h {};

	}

	using namespace Internal;


	HMODULE GetDll_ntdll()
	{
		InitOnFirstUse(&a_ntdll_initFlag,
			[] { a_ntdll_h = LoadLibraryExW(L"ntdll.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); } );

		return a_ntdll_h;
	}


	#pragma warning (disable: 4191)	// 'type cast': unsafe conversion from 'FARPROC' to '...'
	ATDLL_GETFUNC_IMPL(ntdll, RtlNtStatusToDosError)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4StringToAddressA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4StringToAddressW)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6StringToAddressA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6StringToAddressW)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4AddressToStringA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4AddressToStringW)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4AddressToStringExA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv4AddressToStringExW)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6AddressToStringA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6AddressToStringW)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6AddressToStringExA)
	ATDLL_GETFUNC_IMPL(ntdll, RtlIpv6AddressToStringExW)
	ATDLL_GETFUNC_IMPL(ntdll, NtQuerySystemInformation)


	bool TryCall_RtlNtStatusToDosError(NTSTATUS st, int64& err)
	{
		FuncType_RtlNtStatusToDosError fn = GetFunc_RtlNtStatusToDosError();
		if (!fn) return false;
		err = fn(st);
		return true;
	}


	LONG Call_RtlIpv4StringToAddressA(PCSTR a, BOOLEAN b, LPCSTR* c, struct in_addr* d)
	{
		FuncType_RtlIpv4StringToAddressA fn = GetFunc_RtlIpv4StringToAddressA();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}
	
	
	LONG Call_RtlIpv4StringToAddressW(PCWSTR a, BOOLEAN b, LPCWSTR* c, struct in_addr* d)
	{
		FuncType_RtlIpv4StringToAddressW fn = GetFunc_RtlIpv4StringToAddressW();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	LONG Call_RtlIpv6StringToAddressA(PCSTR a, PCSTR* b, struct in6_addr* c)
	{
		FuncType_RtlIpv6StringToAddressA fn = GetFunc_RtlIpv6StringToAddressA();
		if (fn) return fn(a, b, c);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	LONG Call_RtlIpv6StringToAddressW(PCWSTR a, PCWSTR* b, struct in6_addr* c)
	{
		FuncType_RtlIpv6StringToAddressW fn = GetFunc_RtlIpv6StringToAddressW();
		if (fn) return fn(a, b, c);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	PSTR Call_RtlIpv4AddressToStringA(struct in_addr const* a, PSTR b)
	{
		FuncType_RtlIpv4AddressToStringA fn = GetFunc_RtlIpv4AddressToStringA();
		if (fn) return fn(a, b);
		throw NotImplemented();
	}
	

	PWSTR Call_RtlIpv4AddressToStringW(struct in_addr const* a, PWSTR b)
	{
		FuncType_RtlIpv4AddressToStringW fn = GetFunc_RtlIpv4AddressToStringW();
		if (fn) return fn(a, b);
		throw NotImplemented();
	}
	

	LONG Call_RtlIpv4AddressToStringExA(struct in_addr const* a, USHORT b, PSTR c, PULONG d)
	{
		FuncType_RtlIpv4AddressToStringExA fn = GetFunc_RtlIpv4AddressToStringExA();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	LONG Call_RtlIpv4AddressToStringExW(struct in_addr const* a, USHORT b, PWSTR c, PULONG d)
	{
		FuncType_RtlIpv4AddressToStringExW fn = GetFunc_RtlIpv4AddressToStringExW();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	PSTR Call_RtlIpv6AddressToStringA(struct in6_addr const* a, PSTR b)
	{
		FuncType_RtlIpv6AddressToStringA fn = GetFunc_RtlIpv6AddressToStringA();
		if (fn) return fn(a, b);
		throw NotImplemented();
	}
	

	PSTR Call_RtlIpv6AddressToStringW(struct in6_addr const* a, PWSTR b)
	{
		FuncType_RtlIpv6AddressToStringW fn = GetFunc_RtlIpv6AddressToStringW();
		if (fn) return fn(a, b);
		throw NotImplemented();
	}
	

	LONG Call_RtlIpv6AddressToStringExA(struct in6_addr const* a, ULONG b, USHORT c, PSTR d, PULONG e)
	{
		FuncType_RtlIpv6AddressToStringExA fn = GetFunc_RtlIpv6AddressToStringExA();
		if (fn) return fn(a, b, c, d, e);
		return STATUS_NOT_IMPLEMENTED;
	}
	

	LONG Call_RtlIpv6AddressToStringExW(struct in6_addr const* a, ULONG b, USHORT c, PWSTR d, PULONG e)
	{
		FuncType_RtlIpv6AddressToStringExW fn = GetFunc_RtlIpv6AddressToStringExW();
		if (fn) return fn(a, b, c, d, e);
		return STATUS_NOT_IMPLEMENTED;
	}


	NTSTATUS Call_NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS a, PVOID b, ULONG c, PULONG d)
	{
		FuncType_NtQuerySystemInformation fn = GetFunc_NtQuerySystemInformation();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}

}
