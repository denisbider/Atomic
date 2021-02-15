#pragma once

#include "AtAuxStorage.h"
#include "AtSlice.h"


namespace At
{

	// PinStore is an efficient manager of Str objects that never allows underlying memory to move,
	// until all allocated memory is freed in the destructor, at once. This can be used to efficiently
	// store a large number of temporarily built strings that can be referenced elsewhere with Seq
	// as long as the PinStore remains active.

	class PinStore : public AuxStorage
	{
	public:
		PinStore(sizet bytesPerStr) : m_bytesPerStr(bytesPerStr) {}

		sizet BytesPerStr() const { return m_bytesPerStr; }
		void SetBytesPerStr(sizet bytesPerStr) { m_bytesPerStr = bytesPerStr; }

		Enc& GetEnc(sizet writeBytes);
		Enc::Write IncWrite(sizet writeBytes) { return GetEnc(writeBytes).IncWrite(writeBytes); }

		Seq AddStr(Seq s);
		Seq AddStrFromParts(Slice<Seq> parts);
		Seq AddStrFromParts(Seq a, Seq b)        { Seq parts[2] = { a, b };    return AddStrFromParts(parts); }
		Seq AddStrFromParts(Seq a, Seq b, Seq c) { Seq parts[3] = { a, b, c }; return AddStrFromParts(parts); }

		void MergeFrom(PinStore& x) { m_strs.MergeFrom(x.m_strs); m_largeStrs.MergeFrom(x.m_largeStrs); }

	private:
		sizet    m_bytesPerStr {};
		Vec<Str> m_strs;
		Vec<Str> m_largeStrs;
	};

}
