#pragma once

#include "AtStr.h"


namespace At
{

	struct ObjId
	{
		ObjId() = default;
		ObjId(uint64 uniqueId, uint64 index) noexcept : m_uniqueId(uniqueId), m_index(index) {}

		static ObjId None;
		static ObjId Root;

		static ObjId Next(ObjId x) noexcept { ObjId k(x); ++k.m_index; return k; }

		bool Any() const noexcept { return *this != ObjId::None; }

		bool operator<  (ObjId x) const noexcept { return m_uniqueId < x.m_uniqueId || (m_uniqueId == x.m_uniqueId && m_index < x.m_index); }
		bool operator== (ObjId x) const noexcept { return m_uniqueId == x.m_uniqueId && m_index == x.m_index; }
		bool operator<= (ObjId x) const noexcept { return operator<(x) || operator==(x); }
		bool operator!= (ObjId x) const noexcept { return !operator==(x); }

		enum { EncodedSize = 16 };

		byte*       EncodeBin_Ptr  (byte* p) const;
		void        EncodeBin      (Enc& enc) const;
		bool        DecodeBin      (Seq& s);
		static bool SkipDecodeBin  (Seq& s);
	
		void        EncObj     (Enc& s) const;
		bool        ReadStr    (Seq& s);

		uint64 m_uniqueId {};
		uint64 m_index    {};
	};

}
