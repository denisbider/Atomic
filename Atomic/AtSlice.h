#pragma once

#include "AtEnsure.h"


namespace At
{

	template <class T>
	class Slice
	{
	public:
		Slice() {}
		Slice(Slice<T> const& x) : m_begin(x.m_begin), m_end(x.m_end) {}
		Slice(T const* b, T const* e) : m_begin(b), m_end(e) { EnsureThrow(m_begin <= m_end); }

		template <sizet N>
		Slice(T const (&x)[N]) : m_begin(x), m_end(x+N) {}
		
		template <class Container>
		Slice(Container const& x) : m_begin(x.begin()), m_end(x.end()) {}

		Slice<T>& Set(T const* b, T const* e) { EnsureThrow(b <= e); m_begin = b; m_end = e; return *this; }

		Slice<T>& operator= (Slice<T> const& x) { m_begin = x.m_begin; m_end = x.m_end; return *this; }
		
		template <class Container>
		Slice<T>& operator= (Container const& x) { m_begin = x.begin(); m_end = x.end(); }

		T const* begin() const { return m_begin; }
		T const* end() const { return m_end; }
		bool Any() const { return m_begin != m_end; }
		sizet Len() const { return (sizet) (m_end - m_begin); }
		T const& operator[] (sizet i) const { EnsureThrow(i < Len()); return m_begin[i]; }
		T const& First() const { EnsureThrow(Any()); return m_begin[0]; }
		T const& Last() const { EnsureThrow(Any()); return *(m_end-1); }

		Slice<T>& Clear() { m_begin = nullptr; m_end = nullptr; return *this; }
		Slice<T>& PopFirst() { EnsureThrow(Any()); ++m_begin; return *this; }
		Slice<T>& PopLast() { EnsureThrow(Any()); --m_end; return *this; }

	private:
		T const* m_begin {};
		T const* m_end   {};
	};

}
