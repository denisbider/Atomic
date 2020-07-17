#include "AtIncludes.h"
#include "AtEnsureFailDesc.h"


namespace At
{

	ptrdiff volatile EnsureFailDesc::s_nrAllocated {};
	ptrdiff volatile EnsureFailDesc::s_nrFreed {};


	EnsureFailDesc* EnsureFailDesc::Create() noexcept
	{
		LPVOID pv = VirtualAlloc(nullptr, sizeof(EnsureFailDesc), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!pv)
			return nullptr;

		InterlockedIncrement_PtrDiff(&s_nrAllocated);
		return new (pv) EnsureFailDesc(pv);
	}


	void EnsureFailDesc::Free(EnsureFailDesc const* x) noexcept
	{
		if (x)
		{
			void* pv = x->m_pv;
			x->~EnsureFailDesc();

			if (pv)
			{
				VirtualFree(pv, sizeof(EnsureFailDesc), MEM_RELEASE);
				InterlockedIncrement_PtrDiff(&s_nrFreed);
			}
		}
	}


	EnsureFailDescRef& EnsureFailDescRef::Add(byte const* p, sizet n) noexcept
	{
		if (n)
		{
			sizet nRoomLeft = RoomLeftBytes();
			if (nRoomLeft)
			{
				sizet nCopy = PickMin(nRoomLeft, n);
				memcpy(m_efd->m_desc + m_efd->m_descLen, p, nCopy);
				m_efd->m_descLen += nCopy;
				m_efd->SetNullTerm();
			}
		}

		return *this;
	}


	EnsureFailDescRef& EnsureFailDescRef::UInt(uint64 v, uint base, uint zeroPadWidth) noexcept
	{
		if (RoomLeftBytes())
		{
			enum { MaxAddBytes = 99 };
			if (zeroPadWidth < MaxAddBytes)
			{
				byte buf[MaxAddBytes];
				sizet n = UInt2Buf(v, buf, base, zeroPadWidth, CharCase::Upper, 0);
				Add(buf, n);
			}
		}

		return *this;
	}


	EnsureFailDescRef& EnsureFailDescRef::UIntHex(uint64 v) noexcept
	{
		uint zeroPadWidth = ((v <= UINT32_MAX) ? 8U : 16U);
		return Add("0x").UInt(v, 16, zeroPadWidth);
	}


	EnsureFailDescRef& EnsureFailDescRef::UIntMaybeHex(uint64 v) noexcept
	{
		if (v <= UINT16_MAX)
			return UInt(v);
		else
			return UIntHex(v);
	}
	
	
	EnsureFailDescRef& EnsureFailDescRef::SInt(int64 v, AddSign::E addSign, uint base, uint zeroPadWidth) noexcept
	{
		if (RoomLeftBytes())
		{
			enum { MaxAddBytes = 99 };
			if (zeroPadWidth < MaxAddBytes)
			{
				byte buf[MaxAddBytes];
				sizet n = SInt2Buf(v, buf, addSign, base, zeroPadWidth, CharCase::Upper);
				Add(buf, n);
			}
		}

		return *this;
	}


	sizet EnsureFailDescRef::RoomLeftBytes() const noexcept
	{
		if (!m_efd)
			return 0;

		return SatSub<sizet>(EnsureFailDesc::DescBufBytes - 1, m_efd->m_descLen);
	}

}
