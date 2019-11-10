#pragma once

#include "AtEnsure.h"
#include "AtVec.h"


namespace At
{

	class RefCountable
	{
	protected:
		static sizet const INIT_REFCOUNT	= (SIZE_MAX/8)*3;		// Must not equal DESTROY_REFCOUNT
		static sizet const DESTROY_REFCOUNT	= (SIZE_MAX/8)*2;		// Must be less than INIT_REFCOUNT, with room to spare

		LONG64 volatile m_refCount { INIT_REFCOUNT };

	public:
		RefCountable() noexcept {}
		RefCountable(RefCountable const&) noexcept {}
		RefCountable(RefCountable const&&) noexcept {}

		virtual ~RefCountable() noexcept { EnsureAbort(m_refCount == DESTROY_REFCOUNT || m_refCount == INIT_REFCOUNT); }

		RefCountable& operator= (RefCountable const&) noexcept { return *this; }
		RefCountable& operator= (RefCountable const&&) noexcept { return *this; }

		void AddRef() noexcept { if (m_refCount == INIT_REFCOUNT) m_refCount = 1; else InterlockedIncrement64(&m_refCount); }
		void Release() noexcept { EnsureAbort(m_refCount > 0 && m_refCount < INIT_REFCOUNT); if (!InterlockedDecrement64(&m_refCount)) { m_refCount = DESTROY_REFCOUNT; delete this; } }
	};


	template <class T>
	class Rc : public RefCountable, public T
	{
	public:
		Rc() = default;
		Rc(Rc<T> const&) = default;
		Rc(Rc<T>&&) = default;

		template <typename... Args>
		Rc(Args&&... args) : T(std::forward<Args>(args)...) {}

		Rc<T>& operator=(Rc<T> const&) = default;
		Rc<T>& operator=(Rc<T>&&) = default;
	};


	template <class T>
	class Rp
	{
	public:
		// Constructors
		Rp() = default;
		Rp(T* p) noexcept : m_p(p) { if (m_p != nullptr) m_p->AddRef(); }
	
		template <class RelatedType>
		Rp(Rp<RelatedType> const& sp) noexcept { Set(sp); }

		Rp(Rp<T> const& sp) noexcept { Set(sp); }
		Rp(Rp<T>&& sp) noexcept { Set(std::move(sp)); }

		// Destructor
		~Rp() noexcept { Set(nullptr); }

		// Copy operator
		Rp<T>& operator= (T* p) noexcept { Set(p); return *this; }
	
		template <class RelatedType>
		Rp<T>& operator= (Rp<RelatedType> const& sp) noexcept { Set(sp); return *this; }

		Rp<T>& operator= (Rp<T> const& sp) noexcept { Set(sp); return *this; }
		Rp<T>& operator= (Rp<T>&& sp) noexcept { Set(std::move(sp)); return *this; }

		// Set
		void Set(T* p) noexcept { AddRefAndRelease(m_p, p); m_p = p; }
	
		template <class RelatedType>
		void Set(Rp<RelatedType> const& x) noexcept
		{
			AddRefAndRelease(m_p, x.Ptr());
			m_p = dynamic_cast<T*>(x.Ptr());
			if (!m_p && x.Any())
				x.Ptr()->Release();		// dynamic_cast failed
		}
	
		template <>
		void Set(Rp<T> const& x) noexcept
		{
			AddRefAndRelease(m_p, x.Ptr());
			m_p = x.Ptr();
		}

		void Set(Rp<T>&& x) noexcept
		{
			if (m_p) m_p->Release();
			m_p = x.m_p;
			x.m_p = nullptr;
		}

		// Reset: since operator= is overloaded to two pointer types, "*this = 0" is ambiguous
		void Clear() noexcept { Set(nullptr); }

		// Swap
		void Swap(Rp<T>& x) noexcept { NoExcept(std::swap(m_p, x.m_p)); }

		// Get
		T* Ptr() const noexcept { return m_p; }
		T* operator-> () const noexcept { return m_p; }
		T& Ref() const { EnsureThrow(m_p != nullptr); return *m_p; }

		bool Any() const noexcept { return m_p != nullptr; }
	
		// Cast
		template <class U>
		bool IsType() const noexcept { if (!m_p) return false; return dynamic_cast<U*>(m_p) != nullptr; }

		template <class U>
		U* DynamicCast() const noexcept { if (!m_p) return nullptr; return dynamic_cast<U*>(m_p); }

	private:
		T* m_p {};

		static void AddRefAndRelease(RefCountable* curPtr, RefCountable* newPtr) noexcept
		{
			if (newPtr != nullptr) newPtr->AddRef();
			if (curPtr != nullptr) curPtr->Release();
		}
	};

}
