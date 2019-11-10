#pragma once

#include "AtNumCast.h"
#include "AtSatCast.h"

namespace At
{
	// Need to define our own min/max due to awkward min/max macros defined in Windows SDK headers.

	template <class T> inline T constexpr PickMin(T a, T b) { return (a < b) ? a : b; }
	template <class T> inline T constexpr PickMax(T a, T b) { return (a > b) ? a : b; }

	template <class T> inline T constexpr PickMin(T a, T b, T c) { return PickMin(a, PickMin(b, c)); }
	template <class T> inline T constexpr PickMax(T a, T b, T c) { return PickMax(a, PickMax(b, c)); }


	// Unsigned integers

	// Cannot define 'byte' - already defined in Windows 8.1 SDK in rpcndr.h.
	// A conflict arises when other code does "using namespace At" and tries to use 'byte'.
	// The conflict arises no matter how 'byte' is defined, even if it's a typedef for the 'byte' defined in rpcndr.h.
	//
	// typedef unsigned char   byte;

	typedef unsigned short	   ushort;
	typedef unsigned int	   uint;
	typedef unsigned long	   ulong;
	typedef unsigned long long ullong;
	typedef          long long  llong;

	typedef uint16_t uint16;
	typedef uint32_t uint32;
	typedef uint64_t uint64;

	typedef int16_t int16;
	typedef int32_t int32;
	typedef int64_t int64;

	typedef size_t sizet;
	typedef ptrdiff_t ptrdiff;

	sizet constexpr SizeHiBit       { ((sizet) 1) << ((sizeof(sizet) * 8) - 1) };
	sizet constexpr SizeNoHiBitMask { SizeHiBit - 1 };


	// Saturating operations
	
	template <class T> inline T SatSub(T a, T b) noexcept { if (a > b) return (a - b); return 0; }
	template <class T> inline T SatAdd(T a, T b) noexcept { if (b <= std::numeric_limits<T>::max() - a) return (a + b); return std::numeric_limits<T>::max(); }
	template <class T> inline T SatMul(T a, T b) noexcept { if (!a || !b) return 0; if (b <= (std::numeric_limits<T>::max() / a)) return (a * b); return std::numeric_limits<T>::max(); }

	template <class T, T a> inline T SatMulConst(T b) noexcept { if (!b) return 0; if (b <= (std::numeric_limits<T>::max() / a)) return (a * b); return std::numeric_limits<T>::max(); }


	// Functions

	bool IsPowerOf2(uint64 n);

	template <class T> inline T RoundUpToMultipleOf(T n, T k)
	{
		if ((n + k - 1) < n) throw OutOfBounds();
		return((n / k) + 1) * k;
	}

	template <typename T> unsigned int NrBitsUsed(T value);

}
