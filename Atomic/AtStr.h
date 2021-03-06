#pragma once

#include "AtEnc.h"
#include "AtRp.h"
#include "AtUtf8Lit.h"


namespace At
{

	class Str : public Enc
	{
	public:
		// CONSTRUCTION
		Str() = default;
		Str(Str const& x) : Enc(x) {}
		Str(Str&& x) noexcept : Enc(std::move(x)) {}

		Str(Seq x)                        { Add(x); }
		Str(char const* ptr)              { Add(ptr); }
		Str(char const* ptr, sizet count) { Add(ptr, count); }
		Str(byte const* ptr, sizet count) { Add(ptr, count); }
		Str(sizet nrNullBytes)            { Bytes(nrNullBytes, 0); }

		static Str NullTerminate(Seq x) { Str s; s.ReserveExact(x.n+1).Add(x).Byte(0); return s; }

		template <class T, typename... Args>
		static Str From(T const& x, Args&&... args) { Str s; x.EncObj(s, std::forward<Args>(args)...); return s; }

		template <typename... Args>
		static Str Join(Args&&... args) { Str s; s.SetAdd(std::forward<Args>(args)...); return s; }

		// SWAP
		Str& Swap(Str& x) noexcept { NoExcept(Vec<byte>::Swap(x)); return *this; }

		// ALLOCATION
		sizet Cap() const { return Vec<byte>::Cap(); }
		Str& FixCap(sizet fix) { Vec<byte>::FixCap(fix); return *this; }

		Str& FixReserve     (sizet c2)         { Vec<byte>::FixReserve     (c2);           return *this; }
		Str& ReserveExact   (sizet c2)         { Vec<byte>::ReserveExact   (c2);           return *this; }
		Str& ReserveAtLeast (sizet c2)         { Vec<byte>::ReserveAtLeast (c2);           return *this; }
		Str& ReserveInc     (sizet inc)        { Vec<byte>::ReserveInc     (inc);          return *this; }
		Str& ResizeAtLeast  (sizet s2, byte x) { Vec<byte>::ResizeAtLeast  (s2, x);        return *this; }
		Str& ResizeAtLeast  (sizet s2, char x) { Vec<byte>::ResizeAtLeast  (s2, (byte) x); return *this; }
		Str& ResizeAtLeast  (sizet s2)         { Vec<byte>::ResizeAtLeast  (s2);           return *this; }
		Str& ResizeExact    (sizet s2, byte x) { Vec<byte>::ResizeExact    (s2, x);        return *this; }
		Str& ResizeExact    (sizet s2, char x) { Vec<byte>::ResizeExact    (s2, (byte) x); return *this; }
		Str& ResizeExact    (sizet s2)         { Vec<byte>::ResizeExact    (s2);           return *this; }
		Str& ResizeInc      (sizet inc)        { Vec<byte>::ResizeInc      (inc);          return *this; }

		// ASSIGN
		Str& Set(Str const& x)                 { Vec<byte>::operator=(x); return *this; }
		Str& Set(Str&& x) noexcept             { return Free().Swap(x); }
		Str& Set(Seq x)                        { return Set(x.p, x.n); }
		Str& Set(char const* ptr)              { return Set(ptr, ZLen(ptr)); }
		Str& Set(char const* ptr, sizet count) { return Set((byte const*) ptr, count); }
		Str& Set(byte const* ptr, sizet count) { Clear().ReserveExact(count).Add(ptr, count); return *this; }

		Str& operator= (Str const& x)    { Vec<byte>::operator=(x); return *this; }
		Str& operator= (Str&& x)         { Vec<byte>::operator=(std::move(x)); return *this; }
		Str& operator= (Seq x)           { return Set(x); }
		Str& operator= (char const* ptr) { return Set(ptr); }

		Str& SetAdd(Seq a, Seq b                                          ) { ReserveExact(a.n+b.n)                        .Set(a).Add(b);                                           return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c                                   ) { ReserveExact(a.n+b.n+c.n)                    .Set(a).Add(b).Add(c);                                    return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c, Seq d                            ) { ReserveExact(a.n+b.n+c.n+d.n)                .Set(a).Add(b).Add(c).Add(d);                             return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c, Seq d, Seq e                     ) { ReserveExact(a.n+b.n+c.n+d.n+e.n)            .Set(a).Add(b).Add(c).Add(d).Add(e);                      return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c, Seq d, Seq e, Seq f              ) { ReserveExact(a.n+b.n+c.n+d.n+e.n+f.n)        .Set(a).Add(b).Add(c).Add(d).Add(e).Add(f);               return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c, Seq d, Seq e, Seq f, Seq g       ) { ReserveExact(a.n+b.n+c.n+d.n+e.n+f.n+g.n)    .Set(a).Add(b).Add(c).Add(d).Add(e).Add(f).Add(g);        return *this; }
		Str& SetAdd(Seq a, Seq b, Seq c, Seq d, Seq e, Seq f, Seq g, Seq h) { ReserveExact(a.n+b.n+c.n+d.n+e.n+f.n+g.n+h.n).Set(a).Add(b).Add(c).Add(d).Add(e).Add(f).Add(g).Add(h); return *this; }

		// MODIFICATION
		Str& ChIfAny(char c)      { if (Any()) Ch  (c); return *this; }
		Str& IfAny(Seq x)         { if (Any()) Add (x); return *this; }
		Str& IfAny(char const* z) { if (Any()) Add (z); return *this; }

		Str& SetEnd            (Seq x, CaseMatch cm) { if (!Seq(*this).EndsWith           (x, cm)) Add(x); return *this; }
		Str& SetEndExact       (Seq x)               { if (!Seq(*this).EndsWithExact      (x    )) Add(x); return *this; }
		Str& SetEndInsensitive (Seq x)               { if (!Seq(*this).EndsWithInsensitive(x    )) Add(x); return *this; }

		Str& TruncUtf8_MaxBytes (sizet n, Seq ellipsis = Utf8::Lit::Ellipsis) { Seq r{*this}; Seq t=r.ReadUtf8_MaxBytes(n); if (r.n > ellipsis.n) ResizeExact(t.n).Add(ellipsis); return *this; }
		Str& TruncUtf8_MaxChars (sizet n, Seq ellipsis = Utf8::Lit::Ellipsis) { Seq r{*this}; Seq t=r.ReadUtf8_MaxChars(n); if (r.n > ellipsis.n) ResizeExact(t.n).Add(ellipsis); return *this; }

		Str& Set_TruncUtf8_MaxBytes (Seq x, sizet n, Seq ellipsis = Utf8::Lit::Ellipsis) { Seq r=x; Seq t=r.ReadUtf8_MaxBytes(n); if (r.n > ellipsis.n) Set(t.n).Add(ellipsis); else Set(x); return *this; }
		Str& Set_TruncUtf8_MaxChars (Seq x, sizet n, Seq ellipsis = Utf8::Lit::Ellipsis) { Seq r=x; Seq t=r.ReadUtf8_MaxChars(n); if (r.n > ellipsis.n) Set(t.n).Add(ellipsis); else Set(x); return *this; }

		// CLEAR
		Str& Clear () noexcept { Vec<byte>::Clear(); return *this; }
		Str& Free  () noexcept { Vec<byte>::Free();  return *this; }

		// ACCESSORS
		byte const* Ptr() const { return Vec<byte>::Ptr(); }
		byte*       Ptr()       { return Vec<byte>::Ptr(); }

		byte const* PtrEnd() const { return Vec<byte>::PtrEnd(); }
		byte*       PtrEnd()       { return Vec<byte>::PtrEnd(); }

		char const* CharPtr() const { return (char const*) Vec<byte>::Ptr(); }
		char*       CharPtr()       { return (char*)       Vec<byte>::Ptr(); }

		char const* CharPtrEnd() const { return (char const*) Vec<byte>::PtrEnd(); }
		char*       CharPtrEnd()       { return (char*)       Vec<byte>::PtrEnd(); }

		bool  Any()   const { return m_len != 0; }
		sizet Len()   const { return m_len; }

		byte const& operator[] (sizet i) const { EnsureThrow(i<m_len); return Ptr()[i]; }
		byte&       operator[] (sizet i)       { EnsureThrow(i<m_len); return Ptr()[i]; }

		byte const& First () const { EnsureThrow(m_len); return Ptr()[0]; }
		byte&       First ()       { EnsureThrow(m_len); return Ptr()[0]; }

		byte const& Last () const  { EnsureThrow(m_len); return Ptr()[m_len-1]; }
		byte&       Last ()        { EnsureThrow(m_len); return Ptr()[m_len-1]; }

		Str& PopLast() { EnsureThrow(m_len); --m_len; return *this; }

		Seq SubSeq(sizet off)              const { EnsureThrow(off <= m_len); return Seq(Ptr() + off, m_len - off); }
		Seq SubSeq(sizet off, sizet count) const { EnsureThrow(off <= m_len); EnsureThrow(count <= m_len - off); return Seq(Ptr() + off, count); }

		// Aliases of Enc methods with Str return types to allow chaining
		Str& Add              (Seq x)                                               { Enc::Add(x);                 return *this; }
		Str& Add              (char const* ptr)                                     { Enc::Add(ptr);               return *this; }
		Str& Add              (char const* ptr, sizet count)                        { Enc::Add(ptr, count);        return *this; }
		Str& Add              (byte const* ptr, sizet count)                        { Enc::Add(ptr, count);        return *this; }
						      																					   
		Str& Ch               (char ch)                                             { Enc::Ch(ch);                 return *this; }
		Str& Byte             (byte ch)                                             { Enc::Byte(ch);               return *this; }
						      																					   
		Str& Utf8Char         (uint c)                                              { Enc::Utf8Char(c);            return *this; }
						      																					   
		Str& Chars            (sizet count, char ch)                                { Enc::Chars(count, ch);       return *this; }
		Str& Bytes            (sizet count, byte ch)                                { Enc::Bytes(count, ch);       return *this; }
						      																					   
		Str& Word16LE         (uint16 v)                                            { Enc::Word16LE(v);            return *this; }
		Str& Word32LE         (uint32 v)                                            { Enc::Word32LE(v);            return *this; }
						      																					   
		Str& Hex              (uint c, CharCase cc = CharCase::Upper)               { Enc::Hex(c, cc);             return *this; }
		Str& Hex              (Seq  s, CharCase cc = CharCase::Upper, byte sep = 0) { Enc::Hex(s, cc, sep);        return *this; }
		Str& Base32           (Seq s)                                               { Enc::Base32(s);              return *this; }
		Str& UrlBase64        (Seq s, Base64::Padding pad)                          { Enc::UrlBase64(s, pad);      return *this; }
		Str& MimeBase64       (Seq s, Base64::Padding pad)                          { Enc::MimeBase64(s, pad);     return *this; }
						      																					   
		Str& Dbl              (double v, uint prec=4)                               { Enc::Dbl(v, prec);           return *this; }
						      																					   
		Str& ErrCode          (int64 v)                                             { Enc::ErrCode(v);             return *this; }
		Str& TzOffset         (int64 v)                                             { Enc::TzOffset(v);            return *this; }

		Str& UIntUnitsEx(uint64 v, Slice<Units::Unit> units, Units::Unit const*& largestFitUnit)
			{ Enc::UIntUnitsEx(v, units, largestFitUnit); return *this; }

		Str& UIntDecGrp       (uint64 v)                                            { Enc::UIntDecGrp(v);          return *this; }
		Str& UIntUnits        (uint64 v, Slice<Units::Unit> units)                  { Enc::UIntUnits(v, units);    return *this; }
		Str& UIntBytes        (uint64 v)                                            { Enc::UIntBytes(v);           return *this; }
		Str& UIntKb           (uint64 v)                                            { Enc::UIntKb(v);              return *this; }
						      																					   
		Str& Lower            (Seq source)                                          { Enc::Lower(source);          return *this; }
		Str& Upper            (Seq source)                                          { Enc::Upper(source);          return *this; }
						      																					   
		Str& UrlDecode        (Seq in)                                              { Enc::UrlDecode(in);          return *this; }
		Str& UrlEncode        (Seq text)                                            { Enc::UrlEncode(text);        return *this; }
						      																					   
		Str& HtmlAttrValue    (Seq v, Html::CharRefs c)                             { Enc::HtmlAttrValue(v, c);    return *this; }
		Str& HtmlElemText     (Seq t, Html::CharRefs c)                             { Enc::HtmlElemText(t, c);     return *this; }
		Str& JsStrEncode      (Seq text)                                            { Enc::JsStrEncode(text);      return *this; }
		Str& CsvStrEncode     (Seq text)                                            { Enc::CsvStrEncode(text);     return *this; }
		Str& CDataEncode      (Seq text)                                            { Enc::CDataEncode(text);      return *this; }
		Str& ImfCommentEncode (Seq text)                                            { Enc::ImfCommentEncode(text); return *this; }

		template <typename SeqOrStr>
		Str& AddN(typename Vec<SeqOrStr> const& container, Seq separator)
			{ Enc::AddN(container, separator); return *this; }

		Str& UInt(uint64 v, uint base=10, uint zeroPadWidth=0, CharCase charCase=CharCase::Upper, uint spacePadWidth=0)
			{ Enc::UInt(v, base, zeroPadWidth, charCase, spacePadWidth); return *this; }

		Str& SInt(int64 v, AddSign::E addSign=AddSign::IfNeg, uint base=10, uint zeroPadWidth=0, CharCase charCase=CharCase::Upper)
			{ Enc::SInt(v, addSign, base, zeroPadWidth, charCase); return *this; }

		Str& HexDump(Seq s, sizet indent=0, sizet bytesPerLine=16, CharCase cc=CharCase::Upper, Seq newLine="\r\n")
			{ Enc::HexDump(s, indent, bytesPerLine, cc, newLine); return *this; }

		template <class T, typename... Args>
		Str& Obj(T const& x, Args&&... args)
			{ x.EncObj(*this, std::forward<Args>(args)...); return *this; }

		template <class F, typename... Args>
		Str& Fun(F f, Args&&... args)
			{ f(*this, std::forward<Args>(args)...); return *this; }
	};


	// This works better with IntelliSense than defining Str::operator Seq().
	// With the conversion operator, IntelliSense shows a bogus error when initializing Seq from Str using initializer syntax, e.g.: Str s; Seq x { s };
	inline Seq::Seq(Str const& x) noexcept : p(x.Ptr()), n(x.Len()) {}
	inline Seq& Seq::operator= (Str const& x) noexcept { p = x.Ptr(); n = x.Len(); return *this; }


	using RcStr = Rc<Str>;


	// ToLower, ToUpper (whole string)
	inline Str ToLower(Seq source) { Str s; s.Lower(source); return s; }
	inline Str ToUpper(Seq source) { Str s; s.Upper(source); return s; }


	// URL encode/decode
	inline Str UrlDecode(Seq in) { Str s; s.UrlDecode(in); return s; }
	inline Str UrlEncode(Seq in) { Str s; s.UrlEncode(in); return s; }


	// Hex encode
	inline Str HexEncode(Seq in, CharCase cc, byte sep = 0) { Str s; s.Hex(in, cc, sep); return s; }
	inline Str HexEncode(Seq in) { return HexEncode(in, CharCase::Lower); }


	// operator >> to transform Seq into Str via one of the above functions
	inline Str operator>> (Seq x, Str (*f)(Seq)) { return f(x); }


	// COMPARISON
	inline bool operator != (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) != 0; }
	inline bool operator != (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) != 0; }
	inline bool operator != (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) != 0; }
	inline bool operator != (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) != 0; }
	inline bool operator != (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) != 0; }

	inline bool operator == (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) == 0; }
	inline bool operator == (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) == 0; }
	inline bool operator == (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) == 0; }
	inline bool operator == (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) == 0; }
	inline bool operator == (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) == 0; }

	inline bool operator <  (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) <  0; }
	inline bool operator <  (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) <  0; }
	inline bool operator <  (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) <  0; }
	inline bool operator <  (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) <  0; }
	inline bool operator <  (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) <  0; }

	inline bool operator <= (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) <= 0; }
	inline bool operator <= (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) <= 0; }
	inline bool operator <= (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) <= 0; }
	inline bool operator <= (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) <= 0; }
	inline bool operator <= (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) <= 0; }

	inline bool operator >  (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) >  0; }
	inline bool operator >  (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) >  0; }
	inline bool operator >  (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) >  0; }
	inline bool operator >  (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) >  0; }
	inline bool operator >  (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) >  0; }

	inline bool operator >= (Str const& l, Str const&   r) { return Seq(l).Compare(r, CaseMatch::Exact) >= 0; }
	inline bool operator >= (Str const& l, Seq          r) { return Seq(l).Compare(r, CaseMatch::Exact) >= 0; }
	inline bool operator >= (Str const& l, char const*  r) { return Seq(l).Compare(r, CaseMatch::Exact) >= 0; }
	inline bool operator >= (Seq          l, Str const& r) { return     l .Compare(r, CaseMatch::Exact) >= 0; }
	inline bool operator >= (char const*  l, Str const& r) { return Seq(l).Compare(r, CaseMatch::Exact) >= 0; }

	bool StrSlice_Equal(Slice<Str> l, Slice<Str> r, CaseMatch cm);



	// SeqTransform
	typedef Str (*SeqTransformer)(Seq);
	template <SeqTransformer F>
	class SeqTransformed : public Seq
	{
	public:
		SeqTransformed(Seq x)
		{
			m_str = F(x);
			p = m_str.Ptr();
			n = m_str.Len();
		}

	private:
		Str m_str;
	};

	using SeqLower = SeqTransformed<ToLower>;
	using SeqUpper = SeqTransformed<ToUpper>;

}
