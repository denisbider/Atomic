#pragma once

#include "AtAscii.h"
#include "AtEnsure.h"
#include "AtUnicode.h"
#include "AtVec.h"


namespace At
{

	// Null-terminated string functions

	template <typename T> sizet ZLen(T const* z);

	template<> inline sizet ZLen(char    const* z) { return strlen(              z); }
	template<> inline sizet ZLen(byte    const* z) { return strlen((char const*) z); }
	template<> inline sizet ZLen(wchar_t const* z) { return wcslen(              z); }

	
	template <typename T> T const* ZChr(T const* z, uint c);

	template<> inline char    const* ZChr(char    const* z, uint c) { return (!z || c>255) ? nullptr :               strchr(              z, (int)     c); }
	template<> inline byte    const* ZChr(byte    const* z, uint c) { return (!z || c>255) ? nullptr : (byte const*) strchr((char const*) z, (int)     c); }
	template<> inline wchar_t const* ZChr(wchar_t const* z, uint c) { return (!z || c>255) ? nullptr :               wcschr(              z, (wchar_t) c); }



	// Character case
	
	inline uint ToLower(uint c) { if ((c > 64) && (c <  91)) return        (c + 32); return c; }
	inline byte ToLower(byte c) { if ((c > 64) && (c <  91)) return (byte) (c + 32); return c; }

	inline uint ToUpper(uint c) { if ((c > 96) && (c < 123)) return        (c - 32); return c; }
	inline byte ToUpper(byte c) { if ((c > 96) && (c < 123)) return (byte) (c - 32); return c; }



	// Line splitting
	
	struct SplitFlags { enum E : uint {
		None         = 0x00,
		TrimLeft     = 0x01,
		TrimRight    = 0x02,
		DiscardEmpty = 0x04,
		Trim = TrimLeft | TrimRight,
	}; };


	class Enc;
	class Str;
	class Time;

	struct Seq
	{
		static char const* const AsciiWsBytes;
		static char const* const HexDecodeSkipBytes;

		byte const* p {};
		sizet       n {};

		Seq()                         noexcept = default;
		Seq(Seq const&  x)            noexcept : p(x.p), n(x.n) {}
		Seq(Enc const&  x)            noexcept;		// Defined inline in AtEnc.h
		Seq(Str const&  x)            noexcept;		// Defined inline in AtStr.h
		Seq(char const* z)            noexcept : p((byte const*) z), n(ZLen(z)) {}
		Seq(void const* pp, sizet nn) noexcept : p((byte const*) pp), n(nn) {}
		Seq(Vec<wchar_t> const& v)    noexcept : p((byte const*) v.Ptr()), n(v.Len()*sizeof(wchar_t)) {}

		Seq& operator= (Seq const& x) noexcept = default;
		Seq& operator= (Enc const& x) noexcept;		// Defined inline in AtEnc.h
		Seq& operator= (Str const& x) noexcept;		// Defined inline in AtStr.h
		Seq& operator= (char const* z) noexcept { p = (byte const*) z; n = ZLen(z); return *this; }

		byte const* begin () const noexcept { return p; }
		byte const* end   () const noexcept { return p + n; }
		byte const* data  () const noexcept { return p; }
		sizet       size  () const noexcept { return n; }
		sizet       Len   () const noexcept { return n; }
		bool        Any   () const noexcept { return n != 0; }

		sizet CalcWidth(sizet precedingWidth=0, sizet tabStop=4) const noexcept;	// precedingWidth and tabStop control how tab stops are applied. Use precedingWidth=0 for first column

		Seq& SetNull() noexcept { p = 0; n = 0; return *this; }
		Seq& Set(char const* pp)           noexcept { p = (byte const*) pp; n = ZLen(pp); return *this; }
		Seq& Set(void const* pp, sizet nn) noexcept { p = (byte const*) pp; n = nn; return *this; }

		bool StartsWithByte         (uint b)                  const noexcept { return n && p[0] == b; }
		bool StartsWith             (Seq x, CaseMatch cm)     const noexcept { return n >= x.n && Seq(p, x.n).Compare(x, cm) == 0; }
		bool StartsWithExact        (Seq x)                   const noexcept { return StartsWith(x, CaseMatch::Exact); }
		bool StartsWithInsensitive  (Seq x)                   const noexcept { return StartsWith(x, CaseMatch::Insensitive); }
		bool StartsWithAnyOf        (char const* bytes)       const noexcept { return n && ZChr(bytes, p[0]); }
		bool StartsWithAnyOfType    (CharCriterion criterion) const noexcept { return n && criterion(p[0]); }

		bool EndsWithByte           (uint b)                  const noexcept { return n && p[n-1] == b; }
		bool EndsWith               (Seq x, CaseMatch cm)     const noexcept { return n >= x.n && Seq(p+(n-x.n), x.n).Compare(x, cm) == 0; }
		bool EndsWithExact          (Seq x)                   const noexcept { return EndsWith(x, CaseMatch::Exact); }
		bool EndsWithInsensitive    (Seq x)                   const noexcept { return EndsWith(x, CaseMatch::Insensitive); }
		bool EndsWithAnyOf          (char const* bytes)       const noexcept { return n && ZChr(bytes, p[n-1]); }
		bool EndsWithAnyOfType      (CharCriterion criterion) const noexcept { return n && criterion(p[n-1]); }

		bool StripPrefix            (Seq x, CaseMatch cm)     noexcept       { if (!StartsWith(x, cm)) return false; DropBytes(x.n); return true; }
		bool StripPrefixExact       (Seq x)                   noexcept       { return StripPrefix(x, CaseMatch::Exact); }
		bool StripPrefixInsensitive (Seq x)                   noexcept       { return StripPrefix(x, CaseMatch::Insensitive); }

		sizet GeneralPurposeHexDecode(byte* decoded, sizet maxBytes, char const* skipInputBytes = HexDecodeSkipBytes) noexcept;
		sizet ReadHexDecodeInto(Enc& enc, sizet maxBytesToDecode = SIZE_MAX, char const* skipInputBytes = Seq::HexDecodeSkipBytes);

		uint   FirstByte                    () noexcept;	// Returns UINT_MAX if no bytes left
		uint   LastByte                     () noexcept;	// Returns UINT_MAX if no bytes left
		uint   ReadByte                     () noexcept;	// Returns UINT_MAX if no bytes left
		uint   ReadHexEncodedByte           () noexcept;	// Returns UINT_MAX if not enough bytes left or the next 2 bytes aren't hexadecimal digits. No bytes are skipped unless read is successful
		uint   ReadUtf8Char                 () noexcept;	// Returns UINT_MAX if not enough bytes left or invalid UTF-8. No bytes are consumed unless read is successful
		Seq    ReadBytes                    (sizet m)        noexcept;
		bool   ReadBytesInto                (void* pRead, sizet m) noexcept;
		Seq    ReadUtf8_MaxBytes            (sizet maxBytes) noexcept;
		Seq    ReadUtf8_MaxChars            (sizet maxChars) noexcept;
		Seq    ReadToByte                   (uint          b,         sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstByteOf            (char const*   bytes,     sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstByteNotOf         (char const*   bytes,     sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstByteOfType        (CharCriterion criterion, sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstByteNotOfType     (CharCriterion criterion, sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstUtf8CharOfType    (CharCriterion criterion, sizet m = SIZE_MAX) noexcept;
		Seq    ReadToFirstUtf8CharNotOfType (CharCriterion criterion, sizet m = SIZE_MAX) noexcept;
		Seq    ReadToString                 (Seq str, CaseMatch cm = CaseMatch::Exact) noexcept;
		Seq    ReadLeadingNewLine           () noexcept;
		uint64 ReadNrUInt64                 (byte   base = 10, uint64 max = UINT64_MAX) noexcept;	// If value exceeds max, returns max. Supports bases 2-36.
		int64  ReadNrSInt64                 (byte   base = 10, int64  min = INT64_MIN, int64 max = INT64_MAX) noexcept;
		uint32 ReadNrUInt32                 (byte   base = 10, uint32 max = UINT32_MAX)     noexcept { return (uint32) ReadNrUInt64 (base, max      ); }
		uint16 ReadNrUInt16                 (byte   base = 10, uint16 max = UINT16_MAX)     noexcept { return (uint16) ReadNrUInt64 (base, max      ); }
		byte   ReadNrByte                   (byte   base = 10, byte   max = 255)            noexcept { return (byte)   ReadNrUInt64 (base, max      ); }
		uint64 ReadNrUInt64Dec              (uint64 max = UINT64_MAX)                       noexcept { return          ReadNrUInt64 (10,   max      ); }
		int64  ReadNrSInt64Dec              (int64  min = INT64_MIN, int64 max = INT64_MAX) noexcept { return          ReadNrSInt64 (10,   min, max ); }
		uint32 ReadNrUInt32Dec              (uint32 max = UINT32_MAX)                       noexcept { return          ReadNrUInt32 (10,   max      ); }
		uint16 ReadNrUInt16Dec              (uint16 max = UINT16_MAX)                       noexcept { return          ReadNrUInt16 (10,   max      ); }
		byte   ReadNrByteDec                (byte   max = 255)                              noexcept { return          ReadNrByte   (10,   max      ); }
		double ReadDouble                   () noexcept;
		Time   ReadIsoStyleTimeStr          () noexcept;

		// If onToken returns false, stops enumeration and returns false. Returns true if onToken is never called, or if it always returned true.
		bool ForEachNonEmptyToken(char const* separatorBytes, std::function<bool(Seq)> onToken) const;

		Seq& DropByte                     ()                                         noexcept { if (n) { ++p; --n; } return *this; }
		Seq& DropBytes                    (sizet         m)                          noexcept { ReadBytes(m); return *this; }
		Seq& DropUtf8_MaxBytes            (sizet         m)                          noexcept { ReadUtf8_MaxBytes(m); return *this; }
		Seq& DropUtf8_MaxChars            (sizet         m)                          noexcept { ReadUtf8_MaxChars(m); return *this; }
		Seq& DropToByte                   (uint          b,     sizet m = SIZE_MAX)  noexcept { ReadToByte(b, m); return *this; }
		Seq& DropToFirstByteOf            (char const*   bytes, sizet m = SIZE_MAX)  noexcept { ReadToFirstByteOf(bytes, m); return *this; }
		Seq& DropToFirstByteNotOf         (char const*   bytes, sizet m = SIZE_MAX)  noexcept { ReadToFirstByteNotOf(bytes, m); return *this; }
		Seq& DropToFirstByteOfType        (CharCriterion crit,  sizet m = SIZE_MAX)  noexcept { ReadToFirstByteOfType(crit, m); return *this; }
		Seq& DropToFirstByteNotOfType     (CharCriterion crit,  sizet m = SIZE_MAX)  noexcept { ReadToFirstByteNotOfType(crit, m); return *this; }
		Seq& DropToFirstUtf8CharOfType    (CharCriterion crit,  sizet m = SIZE_MAX)  noexcept { ReadToFirstUtf8CharOfType(crit, m); return *this; }
		Seq& DropToFirstUtf8CharNotOfType (CharCriterion crit,  sizet m = SIZE_MAX)  noexcept { ReadToFirstUtf8CharNotOfType(crit, m); return *this; }
		Seq& DropToString                 (Seq str, CaseMatch cm = CaseMatch::Exact) noexcept { ReadToString(str, cm); return *this; }
		Seq& DropLeadingNewLine           ()                                         noexcept { ReadLeadingNewLine(); return *this; }

		uint ReadLastUtf8Char () noexcept;	// Returns UINT_MAX if not enough bytes left or invalid UTF-8. No bytes are consumed unless read is successful
		Seq& DropLastUtf8Char () noexcept { ReadLastUtf8Char(); return *this; }
		uint ReadLastByte     () noexcept { if (n) return p[--n]; return UINT_MAX; }
		Seq& DropLastByte     () noexcept { if (n) --n; return *this; }

		bool ContainsByte                 (uint          b,         sizet m = SIZE_MAX) const { return Seq(*this).DropToByte(b, m).Any(); }
		bool ContainsAnyByteOf            (char const*   bytes,     sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstByteOf(bytes, m).Any(); }
		bool ContainsAnyByteNotOf         (char const*   bytes,     sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstByteNotOf(bytes, m).Any(); }
		bool ContainsAnyByteOfType        (CharCriterion criterion, sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstByteOfType(criterion, m).Any(); }
		bool ContainsAnyByteNotOfType     (CharCriterion criterion, sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstByteNotOfType(criterion, m).Any(); }
		bool ContainsAnyUtf8CharOfType    (CharCriterion criterion, sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstUtf8CharOfType(criterion, m).Any(); }
		bool ContainsAnyUtf8CharNotOfType (CharCriterion criterion, sizet m = SIZE_MAX) const { return Seq(*this).DropToFirstUtf8CharNotOfType(criterion, m).Any(); }
		bool ContainsString               (Seq str, CaseMatch cm = CaseMatch::Exact)    const { return Seq(*this).DropToString(str, cm).Any(); }

		bool Equal            (Seq x, CaseMatch cm) const noexcept { return (cm == CaseMatch::Exact) ? EqualExact(x) : EqualInsensitive(x); }
		bool EqualExact       (Seq x)               const noexcept { if (n != x.n) return false; return !memcmp            (p, x.p, n); }
		bool EqualInsensitive (Seq x)               const noexcept { if (n != x.n) return false; return !MemCmpInsensitive (p, x.p, n); }

		bool ConstTimeEqualExact(Seq x) const noexcept;

		int Compare(Seq x, CaseMatch cm) const noexcept;

		Seq CommonPrefix(Seq x, CaseMatch cm) const noexcept { return (cm == CaseMatch::Exact) ? CommonPrefixExact(x) : CommonPrefixInsensitive(x); }
		Seq CommonPrefixExact(Seq x) const noexcept;
		Seq CommonPrefixInsensitive(Seq x) const noexcept;

		bool operator== (char const* z) const noexcept { return operator==(Seq(z)); }
		bool operator!= (char const* z) const noexcept { return operator!=(Seq(z)); }
	
		bool operator== (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) == 0; }
		bool operator!= (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) != 0; }
		bool operator<= (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) <= 0; }
		bool operator>= (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) >= 0; }
		bool operator<  (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) <  0; }
		bool operator>  (Seq x) const noexcept { return Compare(x, CaseMatch::Exact) >  0; }

		Seq ReadToAfterLastByte              (uint b) noexcept;						// Reads nothing if no match found
		Seq ReadToAfterLastByteOf            (char const* bytes) noexcept;			// Reads nothing if no match found
		Seq ReadToAfterLastByteNotOf         (char const* bytes) noexcept;			// Reads nothing if no match found
		Seq ReadToAfterLastUtf8CharOfType    (CharCriterion criterion) noexcept;	// Reads nothing if no match found. Invalid UTF-8 counts as a match
		Seq ReadToAfterLastUtf8CharNotOfType (CharCriterion criterion) noexcept;	// Reads nothing if no match found. Invalid UTF-8 counts as a match

		Seq& DropToAfterLastByte      (uint b) noexcept            { ReadToAfterLastByte      (b);     return *this; }
		Seq& DropToAfterLastByteOf    (char const* bytes) noexcept { ReadToAfterLastByteOf    (bytes); return *this; }
		Seq& DropToAfterLastByteNotOf (char const* bytes) noexcept { ReadToAfterLastByteNotOf (bytes); return *this; }

		Seq TrimLeft () const noexcept { return Seq(*this).DropToFirstUtf8CharNotOfType     (Unicode::IsWhitespace); }
		Seq TrimRight() const noexcept { return Seq(*this).ReadToAfterLastUtf8CharNotOfType (Unicode::IsWhitespace); }
		Seq Trim     () const noexcept { return TrimLeft().TrimRight(); }

		Seq TrimBytesLeft (char const* bytes) const noexcept { return Seq(*this).DropToFirstByteNotOf     (bytes); }
		Seq TrimBytesRight(char const* bytes) const noexcept { return Seq(*this).ReadToAfterLastByteNotOf (bytes); }
		Seq TrimBytes     (char const* bytes) const noexcept { return TrimBytesLeft(bytes).TrimBytesRight(bytes); }

		// Breaks lines on LF. Trims up to one trailing CR. Does not touch or act on other occurrences of CR
		void ForEachLine(std::function<bool (Seq)> onLine) const;

		template <typename SeqOrStr>
		Vec<SeqOrStr> SplitLines(uint splitFlags);

	private:
		static int MemCmpInsensitive(byte const* a, byte const* b, sizet n) noexcept;
	};

}
