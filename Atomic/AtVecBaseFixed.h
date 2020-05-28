#pragma once

#include "AtNum.h"


namespace At
{

	// A base for container implementations. Not to be used directly.
	// Has a fixed maximum capacity that's part of the object itself.
	// Does not construct objects that are not in use.

	template <class T, uint C>
	class VecBaseFixed
	{
	private:
		VecBaseFixed(VecBaseFixed const&) = delete;
		void operator= (VecBaseFixed const&) = delete;

	public:
		using Val = T;

		bool FixedCap() const noexcept { return true; }

		sizet Cap() const noexcept { return C; }

		T const* Ptr() const noexcept { return (T const*) m_storage.u_mem; }
		T*       Ptr()       noexcept { return (T*)       m_storage.u_mem; }

	protected:
		VecBaseFixed() noexcept = default;
		VecBaseFixed(VecBaseFixed<T,C>&& x) noexcept { Swap(x); }

		void FreeMem() noexcept {}

		VecBaseFixed<T,C>& Swap(VecBaseFixed<T,C>& x) noexcept
		{
			if (m_len > x.m_len)
				x.Swap(*this);
			else
			{
				// m_len <= x.m_len
				static_assert(std::is_nothrow_destructible<T>::value, "Cannot provide basic exception safety if destructor can throw");
				static_assert(std::is_nothrow_move_constructible<T>::value, "Cannot provide strong exception safety if move constructor can throw");
				for (sizet i=0; i!=m_len; ++i)
					NoExcept(std::swap(Ptr()[i], x.Ptr()[i]));
				for (sizet i=m_len; i!=x.m_len; ++i)
				{
					new (Ptr()+i) T(x.Ptr()[i]);
					x.Ptr()[i].~T();
				}
				NoExcept(std::swap(m_len, x.m_len));
			}
			return *this;
		}

		void ReserveExact(sizet cap)
		{
			if (cap > C)
				throw OutOfFixedStorage();
		}

	protected:
		union Storage
		{
			T u_mem[C];
			byte u_dummy;

			Storage() noexcept {}	// Avoid calling constructors
			~Storage() noexcept {}	// or destructors
		};

		Storage m_storage;
		sizet m_len {};			// Measured in T objects
	};

}
