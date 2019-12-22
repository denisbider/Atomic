#pragma once

#include "AtNum.h"


namespace At
{

	HMODULE GetDll_ntdll();

	typedef ULONG (__stdcall* FuncType_RtlNtStatusToDosError)(NTSTATUS);
	FuncType_RtlNtStatusToDosError GetFunc_RtlNtStatusToDosError();
	bool TryCall_RtlNtStatusToDosError(NTSTATUS st, int64& err);

	typedef LONG (__stdcall* FuncType_RtlGetVersion)(RTL_OSVERSIONINFOEXW*);
	FuncType_RtlGetVersion GetFunc_RtlGetVersion();
	LONG Call_RtlGetVersion(RTL_OSVERSIONINFOEXW*);

	typedef LONG (__stdcall* FuncType_RtlVerifyVersionInfo)(RTL_OSVERSIONINFOEXW*, ULONG, ULONGLONG);
	FuncType_RtlVerifyVersionInfo GetFunc_RtlVerifyVersionInfo();
	LONG Call_RtlVerifyVersionInfo(RTL_OSVERSIONINFOEXW*, ULONG, ULONGLONG);
		
	typedef LONG (__stdcall* FuncType_RtlIpv4StringToAddressA)(PCSTR, BOOLEAN, LPCSTR*, struct in_addr*);
	FuncType_RtlIpv4StringToAddressA GetFunc_RtlIpv4StringToAddressA();
	LONG Call_RtlIpv4StringToAddressA(PCSTR, BOOLEAN, LPCSTR*, struct in_addr*);

	typedef LONG (__stdcall* FuncType_RtlIpv4StringToAddressW)(PCWSTR, BOOLEAN, LPCWSTR*, struct in_addr*);
	FuncType_RtlIpv4StringToAddressW GetFunc_RtlIpv4StringToAddressW();
	LONG Call_RtlIpv4StringToAddressW(PCWSTR, BOOLEAN, LPCWSTR*, struct in_addr*);

	typedef LONG (__stdcall* FuncType_RtlIpv6StringToAddressA)(PCSTR, PCSTR*, struct in6_addr*);
	FuncType_RtlIpv6StringToAddressA GetFunc_RtlIpv6StringToAddressA();
	LONG Call_RtlIpv6StringToAddressA(PCSTR, PCSTR*, struct in6_addr*);

	typedef LONG (__stdcall* FuncType_RtlIpv6StringToAddressW)(PCWSTR, PCWSTR*, struct in6_addr*);
	FuncType_RtlIpv6StringToAddressW GetFunc_RtlIpv6StringToAddressW();
	LONG Call_RtlIpv6StringToAddressW(PCWSTR, PCWSTR*, struct in6_addr*);

	typedef PSTR (__stdcall* FuncType_RtlIpv4AddressToStringA)(struct in_addr const*, PSTR);
	FuncType_RtlIpv4AddressToStringA GetFunc_RtlIpv4AddressToStringA();
	PSTR Call_RtlIpv4AddressToStringA(struct in_addr const*, PSTR);

	typedef PWSTR (__stdcall* FuncType_RtlIpv4AddressToStringW)(struct in_addr const*, PWSTR);
	FuncType_RtlIpv4AddressToStringW GetFunc_RtlIpv4AddressToStringW();
	PWSTR Call_RtlIpv4AddressToStringW(struct in_addr const*, PWSTR);

	typedef LONG (__stdcall* FuncType_RtlIpv4AddressToStringExA)(struct in_addr const*, USHORT, PSTR, PULONG);
	FuncType_RtlIpv4AddressToStringExA GetFunc_RtlIpv4AddressToStringExA();
	LONG Call_RtlIpv4AddressToStringExA(struct in_addr const*, USHORT, PSTR, PULONG);

	typedef LONG (__stdcall* FuncType_RtlIpv4AddressToStringExW)(struct in_addr const*, USHORT, PWSTR, PULONG);
	FuncType_RtlIpv4AddressToStringExW GetFunc_RtlIpv4AddressToStringExW();
	LONG Call_RtlIpv4AddressToStringExW(struct in_addr const*, USHORT, PWSTR, PULONG);

	typedef PSTR (__stdcall* FuncType_RtlIpv6AddressToStringA)(struct in6_addr const*, PSTR);
	FuncType_RtlIpv6AddressToStringA GetFunc_RtlIpv6AddressToStringA();
	PSTR Call_RtlIpv6AddressToStringA(struct in6_addr const*, PSTR);

	typedef PSTR (__stdcall* FuncType_RtlIpv6AddressToStringW)(struct in6_addr const*, PWSTR);
	FuncType_RtlIpv6AddressToStringW GetFunc_RtlIpv6AddressToStringW();
	PSTR Call_RtlIpv6AddressToStringW(struct in6_addr const*, PWSTR);

	typedef LONG (__stdcall* FuncType_RtlIpv6AddressToStringExA)(struct in6_addr const*, ULONG, USHORT, PSTR, PULONG);
	FuncType_RtlIpv6AddressToStringExA GetFunc_RtlIpv6AddressToStringExA();
	LONG Call_RtlIpv6AddressToStringExA(struct in6_addr const*, ULONG, USHORT, PSTR, PULONG);

	typedef LONG (__stdcall* FuncType_RtlIpv6AddressToStringExW)(struct in6_addr const*, ULONG, USHORT, PWSTR, PULONG);
	FuncType_RtlIpv6AddressToStringExW GetFunc_RtlIpv6AddressToStringExW();
	LONG Call_RtlIpv6AddressToStringExW(struct in6_addr const*, ULONG, USHORT, PWSTR, PULONG);


	struct ActualSystemTimeOfDayInformation // Size=48
	{
		LARGE_INTEGER BootTime; // Size=8 Offset=0
		LARGE_INTEGER CurrentTime; // Size=8 Offset=8
		LARGE_INTEGER TimeZoneBias; // Size=8 Offset=16
		ULONG TimeZoneId; // Size=4 Offset=24
		ULONG Reserved; // Size=4 Offset=28
		ULONGLONG BootTimeBias; // Size=8 Offset=32
		ULONGLONG SleepTimeBias; // Size=8 Offset=40
	};

	enum SYSTEM_INFORMATION_CLASS
	{
		SystemBasicInformation = 0,
		SystemPerformanceInformation = 2,
		SystemTimeOfDayInformation = 3,
		SystemProcessInformation = 5,
		SystemProcessorPerformanceInformation = 8,
		SystemInterruptInformation = 23,
		SystemExceptionInformation = 33,
		SystemRegistryQuotaInformation = 37,
		SystemLookasideInformation = 45
	};
	
	typedef NTSTATUS (__stdcall* FuncType_NtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
	FuncType_NtQuerySystemInformation GetFunc_NtQuerySystemInformation();
	NTSTATUS Call_NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

}
