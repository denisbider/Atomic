#pragma once

#include "AtNumCvt.h"


namespace At
{

	class EnsureFailDescRef;

	class EnsureFailDesc
	{
	public:
		enum { DescBufBytes = (4096 - (3 * sizeof(ptrdiff))) };

		static ptrdiff volatile s_nrAllocated;
		static ptrdiff volatile s_nrFreed;

		// Returns nullptr if memory could not be allocated
		static EnsureFailDesc* Create() noexcept;

	private:
		void* const              m_pv;
		mutable ptrdiff volatile m_refCount {};
		sizet                    m_descLen  {};			// Without null terminator
		byte                     m_desc[DescBufBytes];

		EnsureFailDesc(void* pv) noexcept : m_pv(pv) { SetNullTerm(); }

		void SetNullTerm() noexcept { m_desc[m_descLen] = 0; }

		void AddRef  () const noexcept { InterlockedIncrement_PtrDiff(&m_refCount); }
		void Release () const noexcept { if (!InterlockedDecrement_PtrDiff(&m_refCount)) Free(this); }

		static void Free(EnsureFailDesc const* x) noexcept;

		friend class EnsureFailDescRef;
	};

	static_assert(sizeof(EnsureFailDesc) == 4096, "EnsureFailDesc is intended to fit one virtual memory page");



	class EnsureFailDescRef
	{
	public:
		EnsureFailDescRef() noexcept {}
		EnsureFailDescRef(EnsureFailDesc*          efd ) noexcept { m_efd = efd;     if (m_efd) m_efd->AddRef(); }
		EnsureFailDescRef(EnsureFailDescRef const& x   ) noexcept { m_efd = x.m_efd; if (m_efd) m_efd->AddRef(); }
		EnsureFailDescRef(EnsureFailDescRef&&      x   ) noexcept { m_efd = x.m_efd; x.m_efd = nullptr; }

		~EnsureFailDescRef() noexcept { if (m_efd) m_efd->Release(); m_efd = nullptr; }

		EnsureFailDescRef& operator= (EnsureFailDesc*          efd ) noexcept { if (m_efd) m_efd->Release(); m_efd = efd;     if (m_efd) m_efd->AddRef(); }
		EnsureFailDescRef& operator= (EnsureFailDescRef const& x   ) noexcept { if (m_efd) m_efd->Release(); m_efd = x.m_efd; if (m_efd) m_efd->AddRef(); }
		EnsureFailDescRef& operator= (EnsureFailDescRef&&      x   ) noexcept { if (m_efd) m_efd->Release(); m_efd = x.m_efd; x.m_efd = nullptr; }

		char const* Z() const noexcept { if (m_efd) return (char const*) (m_efd->m_desc); return "EnsureFailDescRef: m_efd == nullptr"; }

		// All below functions perform saturating concatenations. They do nothing if m_efd == nullptr or if there is no room remaining
		EnsureFailDescRef& Add(char const* z) noexcept { if (z) Add((byte const*) z, strlen(z)); return *this; }
		EnsureFailDescRef& Add(byte const* p, sizet n) noexcept;
		EnsureFailDescRef& UInt(uint64 v, uint base=10, uint zeroPadWidth=0) noexcept;
		EnsureFailDescRef& UIntHex(uint64 v) noexcept;
		EnsureFailDescRef& UIntMaybeHex(uint64 v) noexcept;
		EnsureFailDescRef& SInt(int64 v, AddSign::E addSign=AddSign::IfNeg, uint base=10, uint zeroPadWidth=0) noexcept;

	private:
		EnsureFailDesc* m_efd {};

		// Number of bytes that can still be written to m_efd->m_desc after accounting for null terminator
		sizet RoomLeftBytes() const noexcept;
	};

}
