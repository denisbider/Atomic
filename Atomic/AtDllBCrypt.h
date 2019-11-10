#pragma once

#include "AtIncludes.h"


namespace At
{

	HMODULE GetDll_bcrypt();

	typedef NTSTATUS (__stdcall* FuncType_BCryptOpenAlgorithmProvider)(BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG);
	FuncType_BCryptOpenAlgorithmProvider GetFunc_BCryptOpenAlgorithmProvider();
	NTSTATUS Call_BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptCloseAlgorithmProvider)(BCRYPT_ALG_HANDLE, ULONG);
	FuncType_BCryptCloseAlgorithmProvider GetFunc_BCryptCloseAlgorithmProvider();
	NTSTATUS Call_BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptGetProperty)(BCRYPT_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG*, ULONG);
	FuncType_BCryptGetProperty GetFunc_BCryptGetProperty();
	NTSTATUS Call_BCryptGetProperty(BCRYPT_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG*, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptGenRandom)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);
	FuncType_BCryptGenRandom GetFunc_BCryptGenRandom();
	NTSTATUS Call_BCryptGenRandom(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptDestroyKey)(BCRYPT_KEY_HANDLE);
	FuncType_BCryptDestroyKey GetFunc_BCryptDestroyKey();
	NTSTATUS Call_BCryptDestroyKey(BCRYPT_KEY_HANDLE);

	typedef NTSTATUS (__stdcall* FuncType_BCryptGenerateKeyPair)(BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE*, ULONG, ULONG);
	FuncType_BCryptGenerateKeyPair GetFunc_BCryptGenerateKeyPair();
	NTSTATUS Call_BCryptGenerateKeyPair(BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE*, ULONG, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptFinalizeKeyPair)(BCRYPT_KEY_HANDLE, ULONG);
	FuncType_BCryptFinalizeKeyPair GetFunc_BCryptFinalizeKeyPair();
	NTSTATUS Call_BCryptFinalizeKeyPair(BCRYPT_KEY_HANDLE, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptImportKeyPair)(BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE, LPCWSTR, BCRYPT_KEY_HANDLE*, PUCHAR, ULONG, ULONG);
	FuncType_BCryptImportKeyPair GetFunc_BCryptImportKeyPair();
	NTSTATUS Call_BCryptImportKeyPair(BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE, LPCWSTR, BCRYPT_KEY_HANDLE*, PUCHAR, ULONG, ULONG);
	
	typedef NTSTATUS (__stdcall* FuncType_BCryptExportKey)(BCRYPT_KEY_HANDLE, BCRYPT_KEY_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG*, ULONG);
	FuncType_BCryptExportKey GetFunc_BCryptExportKey();
	NTSTATUS Call_BCryptExportKey(BCRYPT_KEY_HANDLE, BCRYPT_KEY_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG*, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptSignHash)(BCRYPT_KEY_HANDLE, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG*, ULONG);
	FuncType_BCryptSignHash GetFunc_BCryptSignHash();
	NTSTATUS Call_BCryptSignHash(BCRYPT_KEY_HANDLE, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG*, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptVerifySignature)(BCRYPT_KEY_HANDLE, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
	FuncType_BCryptVerifySignature GetFunc_BCryptVerifySignature();
	NTSTATUS Call_BCryptVerifySignature(BCRYPT_KEY_HANDLE, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptDestroyHash)(BCRYPT_HASH_HANDLE);
	FuncType_BCryptDestroyHash GetFunc_BCryptDestroyHash();
	NTSTATUS Call_BCryptDestroyHash(BCRYPT_HASH_HANDLE);

	typedef NTSTATUS (__stdcall* FuncType_BCryptCreateHash)(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
	FuncType_BCryptCreateHash GetFunc_BCryptCreateHash();
	NTSTATUS Call_BCryptCreateHash(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptHashData)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
	FuncType_BCryptHashData GetFunc_BCryptHashData();
	NTSTATUS Call_BCryptHashData(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);

	typedef NTSTATUS (__stdcall* FuncType_BCryptFinishHash)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
	FuncType_BCryptFinishHash GetFunc_BCryptFinishHash();
	NTSTATUS Call_BCryptFinishHash(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);

}
