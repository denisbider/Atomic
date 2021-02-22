#pragma once

#include "AtBaseXY.h"
#include "AtHtmlCharRefs.h"
#include "AtNumCvt.h"
#include "AtSeq.h"
#include "AtUnits.h"
#include "AtVec.h"


namespace At
{

	// An Enc is a partial Enc that supports appending only. It is used as a target for writing that does not permit access to previously written data.

	class Enc : protected Vec<byte>
	{
	protected:
		Enc() = default;
		Enc(Enc const& x) : Vec<byte>(x) {}
		Enc(Enc&& x) noexcept : Vec<byte>(std::move(x)) {}

	public:
		// GENERAL

		Enc& ReserveInc(sizet inc) { Vec<byte>::ReserveInc(inc); return *this; }
		sizet Remaining() const { return Cap() - m_len; }

	public:
		// INDIRECT APPENDING

		struct Meter : NoCopy
		{
		public:
			sizet WrittenLen() const { EnsureThrow(m_enc->m_len >= m_originLen); return m_enc->m_len - m_originLen; }
			Seq   WrittenSeq() const { return Seq { m_enc->Ptr() + m_originLen, WrittenLen() }; }
			bool  Met() const { return m_maxTotalLen == m_enc->m_len; }

		protected:
			Meter(Enc& enc, sizet maxWriteLen) : m_enc(&enc), m_originLen(enc.m_len), m_maxTotalLen(enc.m_len + maxWriteLen) {}

			Enc*  m_enc;
			sizet m_originLen;
			sizet m_maxTotalLen;

			friend class Enc;
		};

		// On destruction, the Write object will zero out any memory that could have been written to, but wasn't.
		class Write : public Meter
		{
		public:
			Write(Write&& x) : Meter(std::move(x)), m_basePtr(x.m_basePtr) { x.m_enc = nullptr; }
			~Write() { if (m_enc && m_enc->m_len < m_maxTotalLen) Mem::Zero(Ptr(), m_enc->Remaining()); }

			byte* Ptr() { return m_enc->Ptr() + m_enc->m_len; }
			char* CharPtr() { return (char*) m_enc->Ptr() + m_enc->m_len; }

			void Add(sizet n)
			{
				EnsureAbort(m_enc->Ptr() == m_basePtr);
				EnsureAbort(n <= m_maxTotalLen - m_enc->m_len);
				m_enc->m_len += n;
			}

			void AddSigned(ptrdiff n) { Add(NumCast<sizet>(n)); }

			sizet AddUpTo(byte const* pWrite)
			{
				ptrdiff diff = pWrite - Ptr();
				EnsureAbort(diff >= 0);
				sizet n = (sizet) diff;
				Add(n);
				return n;
			}

		private:
			Write(Enc& enc, sizet maxWriteLen) : Meter(enc, maxWriteLen), m_basePtr(enc.Ptr()) {}

			byte const* const m_basePtr;

			friend class Enc;
		};

		// A call to IncWrite/FixWrite may be followed by zero, one, or more calls to Write::Add. Multiple calls are incremental.
		// Destruction before Write::Add is OK. Calling IncWrite multiple times before Write::Add is OK, and resets the write.
		// However, it is NOT okay to have multiple outstanding Write objects and interleave Write::Add on them.
		Write IncWrite(sizet n) { ReserveInc(n); return Write(*this, n); }
		Write FixWrite(sizet n) { FixReserve(n); return Write(*this, n); }

		// Useful to measure multiple writes and ensure they amount to a certain number of bytes.
		// Needed to enforce correctness of write size calculations.
		Meter IncMeter(sizet n) { ReserveInc(n); return Meter(*this, n); }
		Meter FixMeter(sizet n) { FixReserve(n); return Meter(*this, n); }

		Seq InvokeWriter(sizet maxLen, std::function<byte*(byte*)> writer)
		{
			Write write = IncWrite(maxLen);
			byte* pStart = write.Ptr();
			byte* pWrite = writer(pStart);
			return Seq(pStart, write.AddUpTo(pWrite));
		}

	public:
		// DIRECT APPENDING

		Enc& Add(Seq x)                        { return Add(x.p, x.n); }
		Enc& Add(char const* ptr)              { return Add((byte const*) ptr, ZLen(ptr)); }
		Enc& Add(char const* ptr, sizet count) { return Add((byte const*) ptr, count); }
		Enc& Add(byte const* ptr, sizet count);

		template <typename SeqOrStr>
		Enc& AddN(typename Vec<SeqOrStr> const& container, Seq separator);

		Enc& Ch   (char ch) { return Byte((byte) ch); }
		Enc& Byte (byte ch) { Vec<byte>::ReserveAtLeast(m_len+1); Ptr()[m_len++] = ch; return *this; }

		Enc& Utf8Char(uint c);

		Enc& Chars(sizet count, char ch) { return Bytes(count, (byte) ch); }
		Enc& Bytes(sizet count, byte ch);

		Enc& Word16LE(uint16 v);
		Enc& Word32LE(uint32 v);

		Enc& Hex        (uint c, CharCase cc = CharCase::Upper);
		Enc& Hex        (Seq  s, CharCase cc = CharCase::Upper, byte sep = 0);
		Enc& Base32     (Seq  s)                      { Base32::Encode            (s, *this, Base32::NewLines::None());      return *this; }
		Enc& UrlBase64  (Seq  s, Base64::Padding pad) { Base64::UrlFriendlyEncode (s, *this, pad);                           return *this; }
		Enc& MimeBase64 (Seq  s, Base64::Padding pad) { Base64::MimeEncode        (s, *this, pad, Base64::NewLines::None()); return *this; }

		Enc& HexDump(Seq s, sizet indent=0, sizet bytesPerLine=16, CharCase cc=CharCase::Upper, Seq newLine="\r\n");

		Enc& UInt(uint64 v, uint base=10, uint zeroPadWidth=0, CharCase charCase=CharCase::Upper, uint spacePadWidth=0);
		Enc& SInt(int64 v, AddSign::E addSign=AddSign::IfNeg, uint base=10, uint zeroPadWidth=0, CharCase charCase=CharCase::Upper);
		Enc& Dbl (double v, uint prec=4);

		Enc& ErrCode(int64 v);
		Enc& TzOffset(int64 v) { return SInt(v, AddSign::Always, 10, 4); }

		Enc& UIntDecGrp  (uint64 v);
		Enc& UIntUnitsEx (uint64 v, Slice<Units::Unit> units, Units::Unit const*& largestFitUnit);	// Does not append unit, instead returns it
		Enc& UIntUnits   (uint64 v, Slice<Units::Unit> units);										// Appends unit
		Enc& UIntBytes   (uint64 v) { return UIntUnits(v, Units::Bytes); }
		Enc& UIntKb      (uint64 v) { return UIntUnits(v, Units::kB); }

		Enc& Lower(Seq source);
		Enc& Upper(Seq source);

		Enc& UrlDecode(Seq in);
		Enc& UrlEncode(Seq text);

		Enc& HtmlAttrValue(Seq value, Html::CharRefs charRefs);
		Enc& HtmlElemText(Seq text, Html::CharRefs charRefs);
		Enc& JsStrEncode(Seq text);
		Enc& CsvStrEncode(Seq text);
		Enc& CDataEncode(Seq text);
		Enc& ImfCommentEncode(Seq text);

		template <class T, typename... Args>
		Enc& Obj(T const& x, Args&&... args)
			{ x.EncObj(*this, std::forward<Args>(args)...); return *this; }

		template <class F, typename... Args>
		Enc& Fun(F f, Args&&... args)
			{ f(*this, std::forward<Args>(args)...); return *this; }

	private:
		void ConsumeHtmlCharRefOrAmpersand(Seq& reader, Html::CharRefs charRefs);

		friend struct Seq;	
	};


	// Must use Seq constructor instead of Enc conversion operator to avoid user-defined conversion operator ambiguity on Str
	inline Seq::Seq(Enc const& x) noexcept : p(x.Ptr()), n(x.Len()) {}
	inline Seq& Seq::operator= (Enc const& x) noexcept { p = x.Ptr(); n = x.Len(); return *this; }

}
