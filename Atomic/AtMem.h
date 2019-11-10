#pragma once

#include "AtEnsure.h"


namespace At
{

	// MSDN does not currently document HeapAlloc alignment guarantees. However, there is this:
	//
	// http://stackoverflow.com/questions/2805896/what-alignment-does-heapalloc-use
	// -> http://stackoverflow.com/a/2806310/4889365
	//    -> https://support.microsoft.com/en-us/kb/286470
	//
	// "The Windows heap managers (all versions) have always guaranteed that the heap allocations
	//  have a start address that is 8-byte aligned (on 64-bit platforms the alignment is 16-bytes)."
	//
	// The below implementation is pretty much exactly the same as 

	struct Mem
	{
		static __declspec(noinline) void BadAlloc();

		template <class T> static T* Alloc(size_t n)
			{	EnsureThrow(n < SIZE_MAX / sizeof(T));
				void* p = HeapAlloc(s_processHeap, 0, n*sizeof(T));
				if (!p) BadAlloc();
				return (T*) p; }

		template <class T> static bool ReAllocInPlace(T* p, size_t n)
			{	EnsureThrow(n < SIZE_MAX / sizeof(T));
				return nullptr != HeapReAlloc(s_processHeap, HEAP_REALLOC_IN_PLACE_ONLY, p, n*sizeof(T)); }

		template <class T> static void Free(T* p) noexcept              { EnsureAbort(HeapFree(s_processHeap, 0, p)); }
		template <class T> static void Copy(T* d, T const* s, size_t n) { memcpy(d, s, n*sizeof(T)); }
		template <class T> static void Move(T* d, T const* s, size_t n) { memmove(d, s, n*sizeof(T)); }
		template <class T> static void Zero(T* d, size_t n)             { RtlSecureZeroMemory(d, n*sizeof(T)); }

	private:
		static HANDLE s_processHeap;
	};

}
