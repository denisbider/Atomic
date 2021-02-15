#pragma once

#include "AtMem.h"
#include "AtNum.h"


namespace At
{

	// A base for container implementations. Not to be used directly.
	// Uses an efficient OS allocator to allocate underlying memory. Does not construct objects that are not in use.

	template <class T>
	class VecBaseHeap
	{
	private:
		VecBaseHeap(VecBaseHeap const&) = delete;
		void operator= (VecBaseHeap const&) = delete;

	public:
		using Val = T;

		bool FixedCap() const noexcept { return (m_cap & SizeHiBit) != 0; }

		sizet Cap() const noexcept { return m_cap & SizeNoHiBitMask; }

		T const* Ptr() const noexcept { return u_mem; }
		T*       Ptr()       noexcept { return u_mem; }

	protected:
		VecBaseHeap() noexcept = default;
		VecBaseHeap(VecBaseHeap<T>&& x) noexcept { Swap(x); }
		~VecBaseHeap() noexcept { FreeMem(); }

		void FreeMem() noexcept
		{
			if (Cap())
				Mem::Free<T>(u_mem);

			u_mem = nullptr;
			m_cap = 0;
		}

		VecBaseHeap<T>& Swap(VecBaseHeap<T>& x) noexcept
		{
			NoExcept(std::swap(u_mem, x.u_mem));
			NoExcept(std::swap(m_cap, x.m_cap));
			NoExcept(std::swap(m_len, x.m_len));
			return *this;
		}

		// Fixes capacity with the beginning of the next non-zero ReserveExact. Fix remains in place until FreeMem or destruction.
		// Fix can be overridden with one or more subsequent calls to FixCap before first non-zero ReserveExact.
		void FixCap(sizet fix) noexcept
		{
			EnsureAbort(!Cap());
			u_fix = fix;
			m_cap = SizeHiBit;
		}

		__declspec(noinline) void ReserveExact(sizet newCap)
		{
			sizet curCap { Cap() };
			if (newCap > curCap)
			{
				if (!curCap)
				{
					if (!u_fix)
					{
						u_mem = Mem::Alloc<T>(newCap);
						m_cap = newCap;
					}
					else
					{
						if (newCap <= u_fix)
							newCap = u_fix;
						else
							throw OutOfFixedStorage();

						u_mem = Mem::Alloc<T>(newCap);
						m_cap = SizeHiBit | newCap;
					}
				}
				else
				{
					if (FixedCap())
						throw OutOfFixedStorage();

					if (Mem::ReAllocInPlace<T>(u_mem, newCap))
						m_cap = newCap;
					else
					{
						T* newMem { Mem::Alloc<T>(newCap) };

						if (std::is_trivially_move_constructible<T>::value &&
							std::is_trivially_destructible<T>::value)
						{
							// memcpy is better optimized than a for loop, for types where we can use it
							Mem::Copy<T>(newMem, u_mem, m_len);
						}
						else
						{
							static_assert(std::is_nothrow_destructible<T>::value, "Cannot provide basic exception safety if destructor can throw");
							static_assert(std::is_nothrow_move_constructible<T>::value, "Cannot provide strong exception safety if move constructor can throw");

							for (sizet i=0; i!=m_len; ++i)
							{
								new (newMem+i) T(std::move(u_mem[i]));
								u_mem[i].~T();
							}
						}

						Mem::Zero<T>(u_mem, m_len);
						Mem::Free<T>(u_mem);
						u_mem = newMem;
						m_cap = newCap;
					}
				}
			}
		}

	private:
		union
		{
			sizet u_fix {};			// When m_cap == 0: if non-zero, any allocation will be for this number of objects, and will be fixed capacity
			T*    u_mem;			// When m_cap != 0
		};

		sizet m_cap {};				// Measured in T objects. If hi bit set, capacity is fixed, or will be fixed at time of next allocation

	protected:
		sizet m_len {};				// Measured in T objects
	};

}
