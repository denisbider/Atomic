#pragma once

#include "AtNum.h"
#include "AtStr.h"
#include "AtVec.h"


namespace At
{

	// Objectives of this multi-precision integer implementation:
	// - For use in non-critical supporting algorithms, specifically ECC point decompression (not supported by Windows CNG at this time).
	// - Could use a library such as Crypto++, but this creates project integration issues, and adds a dependency on a large, external moving target.
	// - For non-critical purposes, prefer smaller, simpler code, even at a significant performance cost; compared to optimized code that's larger and more complex.

	struct MpUInt_LongDivide;

	class MpUInt
	{
	public:
		MpUInt() = default;		// Zero
		MpUInt(uint32 v) { Set(v); }
		MpUInt(Seq bin) { ReadBytes(bin); }

		MpUInt& Set(uint32 v);
		MpUInt& SetZero() { m_words.Clear(); return *this; }

		MpUInt& Swap(MpUInt& x) { m_words.Swap(x.m_words); return *this; }

		// Returns the number of significant bits in the big integer value represented by this object.
		sizet NrBits() const;

		// Returns the number of bytes needed to encode the big integer value. If the stored value is zero, returns 1.
		sizet NrBytes() const;

		// Returns the byte at the requested index. The least significant byte has index 0.
		byte GetByte(sizet i) const;

		MpUInt& ReadBytes(Seq bin);
		MpUInt const& WriteBytes(Enc& enc, sizet padToMinBytes = 0) const;

		bool NonZero() const { return m_words.Any(); }
		bool IsZero() const { return !m_words.Any(); }

		int CmpS(uint64 x) const;
		int CmpS(uint32 x) const;

		int CmpM(MpUInt const& x) const { return Cmp_Impl(x.m_words.Ptr(), x.m_words.Len()); }

		bool operator== (uint64 x) const { return CmpS(x) == 0; }
		bool operator!= (uint64 x) const { return CmpS(x) != 0; }
		bool operator<= (uint64 x) const { return CmpS(x) <= 0; }
		bool operator>= (uint64 x) const { return CmpS(x) >= 0; }
		bool operator<  (uint64 x) const { return CmpS(x) <  0; }
		bool operator>  (uint64 x) const { return CmpS(x) >  0; }

		bool operator== (uint32 x) const { return CmpS(x) == 0; }
		bool operator!= (uint32 x) const { return CmpS(x) != 0; }
		bool operator<= (uint32 x) const { return CmpS(x) <= 0; }
		bool operator>= (uint32 x) const { return CmpS(x) >= 0; }
		bool operator<  (uint32 x) const { return CmpS(x) <  0; }
		bool operator>  (uint32 x) const { return CmpS(x) >  0; }

		bool operator== (MpUInt const& x) const { return CmpM(x) == 0; }
		bool operator!= (MpUInt const& x) const { return CmpM(x) != 0; }
		bool operator<= (MpUInt const& x) const { return CmpM(x) <= 0; }
		bool operator>= (MpUInt const& x) const { return CmpM(x) >= 0; }
		bool operator<  (MpUInt const& x) const { return CmpM(x) <  0; }
		bool operator>  (MpUInt const& x) const { return CmpM(x) >  0; }

		MpUInt& Dec();
		MpUInt& Inc();

		// Methods ending in ...S() take a single-precision (scalar) parameter. Methods ending in ...M() take an MpUInt.

		MpUInt Shr(uint32 x) const { MpUInt r; Shr_Impl(x, r); return r; }
		MpUInt Shl(uint32 x) const { MpUInt r; Shl_Impl(x, r); return r; }

		MpUInt AddM(MpUInt const& x) const { MpUInt r; AddM_Impl(x, r); return r; }

		// Fails with EnsureThrow if the number to subtract is larger than *this.
		MpUInt SubM(MpUInt const& x) const { MpUInt r; SubM_Impl(x, r); return r; }

		MpUInt MulS(uint32 x) const { MpUInt r; MulS_Impl(x, r); return r; }

		// Regular long multiplication. The GMP library uses faster algorithms (such as Karatsuba, Toom, FFT) starting with 768-bit and larger numbers.
		// At this time, this class is intended to be used with Elliptic Curve-sized numbers (e.g. 256 to 521 bits), where long multiplication is faster.
		// GMP manual - multiplication algorithms: https://gmplib.org/manual/Multiplication-Algorithms.html
		// MUL_TOOM22_THRESHOLD on different platforms: https://gmplib.org/devel/thres/MUL_TOOM22_THRESHOLD.html
		MpUInt MulM(MpUInt const& x) const { MpUInt r, t1, t2; MulM_Impl(x, r, t1, t2); return r; }

		void DivS(uint32 divisor, MpUInt& quotient, uint32& remainder) const;
		void DivM(MpUInt const& divisor, MpUInt& quotient, MpUInt& remainder) const;

		MpUInt PowModS(uint32 x, MpUInt const& m) const { MpUInt r; PowModS_Impl(x, m, r); return r; }
		MpUInt PowModM(MpUInt const& x, MpUInt const& m) const { MpUInt r; PowModM_Impl(x, m, r); return r; }

	protected:
		enum { WordBytes = sizeof(uint32), WordBits = 8 * WordBytes };
		Vec<uint32> m_words;	// Least significant word first; leading zeros (at end of vector) stripped; zero is represented as empty vector

		sizet NrWords() const { return m_words.Len(); }
		void StripLeadingZeros();

		int Cmp_Impl(uint32 const* words, sizet nrWords) const;

		// These protected implementation methods assume that the result(s) is/are not an alias of *this, or of any of the parameters, or of themselves.
		void Shr_Impl(uint32 x, MpUInt& r) const;
		void Shl_Impl(uint32 x, MpUInt& r) const;

		void AddM_Impl(MpUInt const& x, MpUInt& r) const;
		void SubM_Impl(MpUInt const& x, MpUInt& r) const;

		void MulS_Impl(uint32 x, MpUInt& r) const;
		void MulM_Impl(MpUInt const& x, MpUInt& r, MpUInt& temp1, MpUInt& temp2) const;

		void DivS_Impl(uint32 divisor, MpUInt& quotient, uint32& remainder) const;
		void DivM_Impl(MpUInt const& divisor, MpUInt& quotient, MpUInt& remainder) const;

		void PowModS_Impl(uint32 x, MpUInt const& m, MpUInt& result) const;
		void PowModM_Impl(MpUInt const& x, MpUInt const& m, MpUInt& result) const;

		// Returns the number of significant bytes in the specified word. If the word is zero, returns 0.
		static sizet NrSignificantBytes(uint32 word);

		static uint32 HiWord(uint64 v) { return (uint32) ((v >> 32) & 0xFFFFFFFF); }
		static uint32 LoWord(uint64 v) { return (uint32) ( v        & 0xFFFFFFFF); }

		static uint64 JoinWords(uint32 hi, uint32 lo) { return (((uint64) hi) << 32) | ((uint64) lo); }

		friend struct MpUInt_LongDivide;
	};

	static_assert(std::is_nothrow_move_constructible<MpUInt>::value, "MpUInt is expected to be notrow move constructible");
	static_assert(std::is_nothrow_move_assignable<MpUInt>::value,    "MpUInt is expected to be notrow move assignable");
}
