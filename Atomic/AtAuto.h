#pragma once

#include "AtIncludes.h"

namespace At
{
	template <class T>
	class PtrHolder : NoCopy
	{
	public:
		PtrHolder(T* p = nullptr) noexcept : m_p(p) {}
		PtrHolder(PtrHolder<T>&& x) noexcept : m_p(x.m_p) { x.m_p = nullptr; }

		T*       Ptr()       { return m_p; }
		T const* Ptr() const { return m_p; }

		T&       Ref()       { return *m_p; }
		T const& Ref() const { return *m_p; }

		bool Any() const { return m_p != nullptr; }

		operator T*       ()       { return m_p; }
		operator T const* () const { return m_p; }

		T*       operator-> ()       { return m_p; }
		T const* operator-> () const { return m_p; }

		bool operator== (T* p) const { return m_p == p; }
		bool operator!= (T* p) const { return m_p != p; }

		bool operator! () const { return m_p == nullptr; }

	protected:
		T* m_p;
	};


	template <class T, class F>
	class AutoAction : public PtrHolder<T>
	{
	public:
		AutoAction(T* p = nullptr) noexcept : PtrHolder<T>(p) {}
		AutoAction(AutoAction<T,F>&& x) noexcept : PtrHolder<T>(std::move(x)) {}
		~AutoAction() { if (m_p) F::Action(m_p); }

		void Set(T* p)
		{
			if (m_p) F::Action(m_p);
			m_p = p;
		}

		T* Dismiss() { T* p = m_p; m_p = nullptr; return p; }
	};


	template <class T> struct AutoAction_Free    { static void Action(T* p) { delete p; } };
	template <class T> struct AutoAction_SetZero { static void Action(T* p) { *p = 0; } };

	template <class T> using AutoFree    = AutoAction<T, AutoAction_Free   <T>>;
	template <class T> using AutoSetZero = AutoAction<T, AutoAction_SetZero<T>>;
}
