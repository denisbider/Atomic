#pragma once

#include "AtStr.h"


namespace At
{

	// Encode functions are used to encode binary structures with explicit sizes that have to be calculated in advance.
	// Therefore, usage cases can calculate the full size and pre-allocate the buffer.

	inline void EncodeVerbatim(Enc& enc, Seq s) { enc.Add(s); }

	inline void EncodeByte(Enc& enc, byte v) { enc.Byte(v); }
	bool DecodeByte(Seq& s, uint& b);

	// Fixed-size 16-bit encoding. Preserves sort order.
	// Any high bits not representable in 16-bit encoding are ignored.
	void EncodeUInt16(Enc& enc, unsigned int n);
	bool DecodeUInt16(Seq& s, unsigned int& n);

	// Fixed-size 32-bit encoding. Preserves sort order.
	void EncodeUInt32(Enc& enc, uint32 n);
	bool DecodeUInt32(Seq& s, uint32& n);

	// Preserves sort order, so that 0 < 1 < 2 < ...
	sizet EncodeVarUInt64_Size(uint64 n);
	void EncodeVarUInt64(Enc& enc, uint64 n);
	bool DecodeVarUInt64(Seq& s, uint64& n);

	// Preserves sort order, so that ... < -1 < 0 < 1 < ...
	sizet EncodeVarSInt64_Size(int64 n);
	void EncodeVarSInt64(Enc& enc, int64 n);
	bool CanTryDecodeVarSInt64(Seq s);
	bool DecodeVarSInt64(Seq& s, int64& n);

	// Does NOT preserve sort order
	enum { EncodeDouble_Size = 8 };
	inline void EncodeDouble(Enc& enc, double v) { enc.Add(Seq(&v, 8)); }
	bool DecodeDouble(Seq& s, double& v);

	// Preserves sort order, so that -1.5 < -1.0 < -0.0 < 0.0 < 1.0 < 1.5
	enum { EncodeSortDouble_Size = 8 };
	void EncodeSortDouble(Enc& enc, double v);
	bool DecodeSortDouble(Seq& s, double& v);

	// An efficient string encoding that does NOT preserve sort order. Encoded as length (VarUInt64) followed by string content.
	inline sizet EncodeVarStr_Size(sizet dataLen) { return EncodeVarUInt64_Size(dataLen) + dataLen; }
	void EncodeVarStr(Enc& enc, Seq data);
	bool CanTryDecodeVarStr(Seq s, sizet maxLen = SIZE_MAX);
	bool DecodeVarStr(Seq& s, Seq& data, sizet maxLen = SIZE_MAX);

	// A less efficient string encoding that preserves sort order. Terminated by two consecutive null bytes.
	// Any null bytes within the string are encoded as a null byte followed by another byte whose value is the number of null bytes to encode.
	sizet EncodeSortStr_Size(Seq data);
	void EncodeSortStr(Enc& enc, Seq data);
	bool DecodeSortStr(Seq& s, Str& data);
}
