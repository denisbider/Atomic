#include "AtIncludes.h"
#include "AtDllBCrypt.h"

#include "AtDllGetFunc.h"
#include "AtInitOnFirstUse.h"

#pragma warning (push)
#pragma warning (disable: 4073)		// L3: initializers put in library initialization area
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	namespace Internal
	{
		
		LONG volatile a_bcrypt_initFlag {};
		HMODULE a_bcrypt_h {};

	}

	using namespace Internal;


	HMODULE GetDll_bcrypt()
	{
		InitOnFirstUse(&a_bcrypt_initFlag,
			[] { a_bcrypt_h = LoadLibraryExW(L"bcrypt.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); } );

		return a_bcrypt_h;
	}


	#pragma warning (disable: 4191)	// 'type cast': unsafe conversion from 'FARPROC' to '...'
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptOpenAlgorithmProvider)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptCloseAlgorithmProvider)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptGetProperty)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptSetProperty)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptGenRandom)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptDestroyKey)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptGenerateSymmetricKey)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptEncrypt)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptDecrypt)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptGenerateKeyPair)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptFinalizeKeyPair)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptImportKeyPair)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptExportKey)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptSignHash)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptVerifySignature)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptDestroyHash)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptCreateHash)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptHashData)
	ATDLL_GETFUNC_IMPL(bcrypt, BCryptFinishHash)
	

	NTSTATUS Call_BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE* a, LPCWSTR b, LPCWSTR c, ULONG d)
	{
		FuncType_BCryptOpenAlgorithmProvider fn = GetFunc_BCryptOpenAlgorithmProvider();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE a, ULONG b)
	{
		FuncType_BCryptCloseAlgorithmProvider fn = GetFunc_BCryptCloseAlgorithmProvider();
		if (fn) return fn(a, b);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptGetProperty(BCRYPT_HANDLE a, LPCWSTR b, PUCHAR c, ULONG d, ULONG* e, ULONG f)
	{
		FuncType_BCryptGetProperty fn = GetFunc_BCryptGetProperty();
		if (fn) return fn(a, b, c, d, e, f);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptSetProperty(BCRYPT_HANDLE a, LPCWSTR b, PUCHAR c, ULONG d, ULONG e)
	{
		FuncType_BCryptSetProperty fn = GetFunc_BCryptSetProperty();
		if (fn) return fn(a, b, c, d, e);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptGenRandom(BCRYPT_ALG_HANDLE a, PUCHAR b, ULONG c, ULONG d)
	{
		FuncType_BCryptGenRandom fn = GetFunc_BCryptGenRandom();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptDestroyKey(BCRYPT_KEY_HANDLE a)
	{
		FuncType_BCryptDestroyKey fn = GetFunc_BCryptDestroyKey();
		if (fn) return fn(a);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptGenerateSymmetricKey(BCRYPT_ALG_HANDLE a, BCRYPT_KEY_HANDLE b, PUCHAR c, ULONG d, PUCHAR e, ULONG f, ULONG g)
	{
		FuncType_BCryptGenerateSymmetricKey fn = GetFunc_BCryptGenerateSymmetricKey();
		if (fn) return fn(a, b, c, d, e, f, g);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptEncrypt(BCRYPT_KEY_HANDLE a, PUCHAR b, ULONG c, void* d, PUCHAR e, ULONG f, PUCHAR g, ULONG h, ULONG* i, ULONG j)
	{
		FuncType_BCryptEncrypt fn = GetFunc_BCryptEncrypt();
		if (fn) return fn(a, b, c, d, e, f, g, h, i, j);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptDecrypt(BCRYPT_KEY_HANDLE a, PUCHAR b, ULONG c, void* d, PUCHAR e, ULONG f, PUCHAR g, ULONG h, ULONG* i, ULONG j)
	{
		FuncType_BCryptDecrypt fn = GetFunc_BCryptDecrypt();
		if (fn) return fn(a, b, c, d, e, f, g, h, i, j);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptGenerateKeyPair(BCRYPT_ALG_HANDLE a, BCRYPT_KEY_HANDLE* b, ULONG c, ULONG d)
	{
		FuncType_BCryptGenerateKeyPair fn = GetFunc_BCryptGenerateKeyPair();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptFinalizeKeyPair(BCRYPT_KEY_HANDLE a, ULONG b)
	{
		FuncType_BCryptFinalizeKeyPair fn = GetFunc_BCryptFinalizeKeyPair();
		if (fn) return fn(a, b);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptImportKeyPair(BCRYPT_ALG_HANDLE a, BCRYPT_KEY_HANDLE b, LPCWSTR c, BCRYPT_KEY_HANDLE* d, PUCHAR e, ULONG f, ULONG g)
	{
		FuncType_BCryptImportKeyPair fn = GetFunc_BCryptImportKeyPair();
		if (fn) return fn(a, b, c, d, e, f, g);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptExportKey(BCRYPT_KEY_HANDLE a, BCRYPT_KEY_HANDLE b, LPCWSTR c, PUCHAR d, ULONG e, ULONG* f, ULONG g)
	{
		FuncType_BCryptExportKey fn = GetFunc_BCryptExportKey();
		if (fn) return fn(a, b, c, d, e, f, g);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptSignHash(BCRYPT_KEY_HANDLE a, void* b, PUCHAR c, ULONG d, PUCHAR e, ULONG f, ULONG* g, ULONG h)
	{
		FuncType_BCryptSignHash fn = GetFunc_BCryptSignHash();
		if (fn) return fn(a, b, c, d, e, f, g, h);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptVerifySignature(BCRYPT_KEY_HANDLE a, void* b, PUCHAR c, ULONG d, PUCHAR e, ULONG f, ULONG g)
	{
		FuncType_BCryptVerifySignature fn = GetFunc_BCryptVerifySignature();
		if (fn) return fn(a, b, c, d, e, f, g);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptDestroyHash(BCRYPT_HASH_HANDLE a)
	{
		FuncType_BCryptDestroyHash fn = GetFunc_BCryptDestroyHash();
		if (fn) return fn(a);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptCreateHash(BCRYPT_ALG_HANDLE a, BCRYPT_HASH_HANDLE* b, PUCHAR c, ULONG d, PUCHAR e, ULONG f, ULONG g)
	{
		FuncType_BCryptCreateHash fn = GetFunc_BCryptCreateHash();
		if (fn) return fn(a, b, c, d, e, f, g);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptHashData(BCRYPT_HASH_HANDLE a, PUCHAR b, ULONG c, ULONG d)
	{
		FuncType_BCryptHashData fn = GetFunc_BCryptHashData();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}

	NTSTATUS Call_BCryptFinishHash(BCRYPT_HASH_HANDLE a, PUCHAR b, ULONG c, ULONG d)
	{
		FuncType_BCryptFinishHash fn = GetFunc_BCryptFinishHash();
		if (fn) return fn(a, b, c, d);
		return STATUS_NOT_IMPLEMENTED;
	}


}
