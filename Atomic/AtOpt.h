#pragma once

#include "AtEnsure.h"

namespace At
{
	// Allows the nullability aspect of any class to be separated from its value and functionality.
	// Classes can rely on Opt<> to provide nullability, and not have to worry about providing
	// a default constructor, a null state, and always checking for the null state when accessed.
	// This involves no heap allocation, and overhead is minimal: one additional boolean value.

	template <class T>
	class Opt
	{
	public:
		// Due to VS 2015 bugs, a static_assert requiring nothrow destruction and move construction is preferable over the following more general solution:
		//
		//   ~Opt() noexcept(std::is_nothrow_destructible<T>::value) { ... }
		//   Opt(T&& o) noexcept(std::is_nothrow_move_constructible<T>::value) { ... }
		//
		// Bug #1 - in VS 2015, the following would static assert:
		//
		//   template <class T> struct A { ~A() noexcept(std::is_nothrow_destructible<T>::value) {} };
		//   // A<int> a;
		//   static_assert(std::is_nothrow_destructible<A<int>>::value, "");
		//
		// Uncommenting the commented line would fix the assertion. This bug might have been fixed: as of 2020-05-08, using the above snippet, I could not reproduce it.
		//
		// Bug #2 - in VS 2015, the following will static assert:
		//
		//   template <class T> struct B { B(B&&) noexcept(std::is_nothrow_move_constructible<T>::value) {} };
		//   // static_assert(std::is_nothrow_move_constructible<B<int>>::value, "");
		//   struct C { B<int> b; };
		//   static_assert(std::is_nothrow_move_constructible<C>::value, "");
		//
		// Uncommenting the commented line fixes the assertion. As of 2020-05-08, this remains reproducible.

		static_assert(std::is_nothrow_destructible<T>::value, "Throwing destructor not supported. See comment above");
		static_assert(std::is_nothrow_move_constructible<T>::value, "Throwing move constructor not supported. See comment above");

		enum ECreate { Create };

		Opt()                noexcept                                                  : m_any(false)   { }
		Opt(ECreate)         noexcept(std::is_nothrow_default_constructible<T>::value) : m_any(true)    {              new (&m_u.x) T();                                }
		Opt(T const& o)      noexcept(std::is_nothrow_copy_constructible<T>::value)    : m_any(true)    {              new (&m_u.x) T(o);                               }
		Opt(T&&      o)      noexcept                                                  : m_any(true)    {              new (&m_u.x) T(std::move(o));                    }
		Opt(Opt<T> const& o) noexcept(std::is_nothrow_copy_constructible<T>::value)    : m_any(o.m_any) { if (m_any)   new (&m_u.x) T(o.m_u.x);                         }
		Opt(Opt<T>&&      o) noexcept                                                  : m_any(o.m_any) { if (m_any) { new (&m_u.x) T(std::move(o.m_u.x)); o.Clear(); } }

		template <typename... Args>
		Opt(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value) : m_any(true) { new (&m_u.x) T(args...); }

		~Opt() noexcept { Clear(); }

		Opt<T>& operator= (T const&      o) noexcept(std::is_nothrow_copy_assignable<T>::value) { Clear();                             new (&m_u.x) T(o);                               return *this; }
		Opt<T>& operator= (T&&           o) noexcept(std::is_nothrow_move_assignable<T>::value) { Clear();                             new (&m_u.x) T(std::move(o));                    return *this; }
		Opt<T>& operator= (Opt<T> const& o) noexcept(std::is_nothrow_copy_assignable<T>::value) { Clear(); m_any=o.m_any; if (m_any)   new (&m_u.x) T(o.m_u.x);                         return *this; }
		Opt<T>& operator= (Opt<T>&&      o) noexcept(std::is_nothrow_move_assignable<T>::value) { Clear(); m_any=o.m_any; if (m_any) { new (&m_u.x) T(std::move(o.m_u.x)); o.Clear(); } return *this; }

		template <typename... Args>
		T& Init(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
		{
			EnsureThrow(!m_any);
			new (&m_u.x) T(std::forward<Args>(args)...);
			m_any = true;
			return m_u.x;
		}
		
		Opt<T>& Clear() noexcept(std::is_nothrow_destructible<T>::value)
		{
			if (m_any)
			{
				m_any = false;
				m_u.x.~T();
			}
			return *this;
		}

		template <typename... Args>
		T& ReInit(Args&&... args) noexcept(std::is_nothrow_destructible<T>::value && std::is_nothrow_constructible<T, Args...>::value)
			{ return Clear().Init(std::forward<Args>(args)...); }

		bool Any() const noexcept { return m_any; }

		T*       Ptr()               { if (m_any) return &m_u.x; return nullptr; }
		T const* Ptr()         const { if (m_any) return &m_u.x; return nullptr; }

		T&       Ref()               { EnsureThrow(m_any); return m_u.x; }
		T const& Ref()         const { EnsureThrow(m_any); return m_u.x; }

		T*       operator-> ()       { EnsureThrow(m_any); return &m_u.x; }
		T const* operator-> () const { EnsureThrow(m_any); return &m_u.x; }

	private:
		struct Dummy {};

		template <class T>
		union U
		{
			T     x;
			Dummy d;

			U() : d() {}
			~U() { d.~Dummy(); }
		};

		U<T> m_u;
		bool m_any {};
	};
}
