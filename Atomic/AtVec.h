#pragma once

#include "AtVecBaseFixed.h"
#include "AtVecBaseHeap.h"

#include "AtSlice.h"


namespace At
{

	// A vector that:
	// - Provides a strong exception safety guarantee: if any method throws, logical container state will be as before method call.
	//   Underlying representation may be different - e.g. elements may have been moved to a new memory location.
	// - Requires elements to be nothrow move constructible, and nothrow destructible. These are the only requirements,
	//   and are sufficient to provide the strong exception safety guarantee.

	template <class B>
	class VecCore : public B
	{
	public:
		using Val = typename B::Val;

		static constexpr sizet ValSize() { return sizeof(typename Val); }	// allows vec.ValSize() instead of sizeof(Vec<Complicated<Type>*>::Val)

		VecCore() = default;
		VecCore(VecCore<B>&& x) noexcept : B(std::move(x)) {}
		VecCore(VecCore<B> const& x) { AddSlice(x); }
		VecCore(Slice<Val> x) { AddSlice(x); }

		~VecCore() noexcept { Clear(); }

		VecCore<B>& operator= (VecCore<B> const& x) { VecCore<B> temp(x); return operator=(std::move(temp)); }	// Copy into temp and swap required for strong exception safety
		VecCore<B>& operator= (VecCore<B>&& x) noexcept { return NoExcept(Free().Swap(x)); }

		sizet Len() const noexcept { return m_len; }

		bool Any() const noexcept { return m_len != 0; }

		Val const* begin() const noexcept { return Ptr(); }
		Val*       begin()       noexcept { return Ptr(); }

		Val const* end() const   noexcept { return Ptr() + m_len; }
		Val*       end()         noexcept { return Ptr() + m_len; }

		Slice<Val> GetSlice(sizet i, sizet n = SIZE_MAX) const noexcept
			{ if (i > m_len) i = m_len; if (n > m_len - i) n = m_len - i; return Slice<Val>(Ptr()+i, Ptr()+i+n); }
		
		Val&       operator[] (sizet i)       { EnsureThrowWithNr(i < m_len, i); return Ptr()[i]; }
		Val const& operator[] (sizet i) const { EnsureThrowWithNr(i < m_len, i); return Ptr()[i]; }

		Val&       First()       { EnsureThrow(m_len >= 1); return Ptr()[0]; }
		Val const& First() const { EnsureThrow(m_len >= 1); return Ptr()[0]; }

		Val&       Last()        { EnsureThrow(m_len >= 1); return Ptr()[m_len - 1]; }
		Val const& Last() const  { EnsureThrow(m_len >= 1); return Ptr()[m_len - 1]; }

		sizet FindIdx(std::function<bool (Val& x)> criterion)
		{
			for (sizet i=0; i!=m_len; ++i)
				if (criterion(Ptr()[i]))
					return i;
			return SIZE_MAX;
		}

		sizet FindIdx(std::function<bool (Val const& x)> criterion) const
		{
			for (sizet i=0; i!=m_len; ++i)
				if (criterion(Ptr()[i]))
					return i;
			return SIZE_MAX;
		}

		Val*       FindPtr(std::function<bool (Val&       x)> criterion)       { sizet i = FindIdx(criterion); if (i == SIZE_MAX) return nullptr; return Ptr() + i; }
		Val const* FindPtr(std::function<bool (Val const& x)> criterion) const { sizet i = FindIdx(criterion); if (i == SIZE_MAX) return nullptr; return Ptr() + i; }

		Val& FindRef(std::function<bool (Val& x)> criterion)
		{
			sizet i = FindIdx(criterion);
			EnsureThrow(i != SIZE_MAX);
			return Ptr()[i];
		}

		Val const& FindRef(std::function<bool (Val const& x)> criterion) const
		{
			sizet i = FindIdx(criterion);
			EnsureThrow(i != SIZE_MAX);
			return Ptr()[i];
		}

		// Returns true if found and erased, false if not found
		bool FindErase(std::function<bool (Val& x)> criterion)
		{
			sizet i = FindIdx(criterion);
			if (i == SIZE_MAX)
				return false;

			Erase(i, 1);
			return true;
		}

		sizet      FindIdx   (Val const& sample) const { return FindIdx   ( [&] (Val const& other) -> bool { return other == sample; } ); }
		Val const* FindPtr   (Val const& sample) const { return FindPtr   ( [&] (Val const& other) -> bool { return other == sample; } ); }
		bool       FindErase (Val const& sample)       { return FindErase ( [&] (Val const& other) -> bool { return other == sample; } ); }

		bool Contains(Val const& sample) const                            { return FindIdx(sample)    != SIZE_MAX; }
		bool Contains(std::function<bool (Val const& x)> criterion) const { return FindIdx(criterion) != SIZE_MAX; }

		bool AddIfNotContains(Val const& sample)		// Returns true if added
		{
			sizet i = FindIdx(sample);
			if (i != SIZE_MAX) return false;
			Add(sample); return true;
		}

		bool AddIfNotContains(Val&& sample)			// Returns true if added
		{
			sizet i = FindIdx(sample);
			if (i != SIZE_MAX) return false;
			Add(std::forward<Val>(sample)); return true;
		}

		VecCore<B>& Clear() noexcept
		{
			if (std::is_trivially_destructible<Val>::value)
				Mem::Zero<Val>(Ptr(), m_len);
			else
			{
				static_assert(std::is_nothrow_destructible<Val>::value, "Cannot provide strong - and during destruction, basic - exception safety if destructor can throw");
				for (sizet i=0; i!=m_len; ++i)
					Ptr()[i].~Val();
			}
			m_len = 0;
			return *this;
		}

		VecCore<B>& FixCap (sizet fix)     noexcept { this->B::FixCap(fix); return *this; }
		VecCore<B>& Free   ()              noexcept { NoExcept(Clear()); NoExcept(this->B::FreeMem()); return *this; }
		VecCore<B>& Swap   (VecCore<B>& x) noexcept { NoExcept(this->B::Swap(x)); return *this; }

		VecCore<B>& SwapAt(sizet i, sizet k)
		{
			EnsureThrowWithNr(i < m_len, i);
			EnsureThrowWithNr(k < m_len, k);

			static_assert(std::is_nothrow_move_constructible<Val>::value, "Cannot provide basic exception safety if move constructor can throw");
			static_assert(std::is_nothrow_destructible<Val>::value, "Cannot provide basic exception safety if destructor can throw");

			Val* pi = Ptr() + i;
			Val* pk = Ptr() + k;

			Val temp(std::move(*pi));
			pi->~Val();
			new (pi) Val(std::move(*pk));
			pk->~Val();
			new (pk) Val(std::move(temp));

			return *this; 
		}

		VecCore<B>& FixReserve(sizet cap)
		{
			this->B::FixCap(cap);
			this->B::ReserveExact(cap);
			return *this;
		}

		VecCore<B>& ReserveExact(sizet cap)
		{
			this->B::ReserveExact(cap);
			return *this;
		}

		VecCore<B>& ReserveInc(sizet inc)
		{
			EnsureThrow(inc < SIZE_MAX - m_len);
			ReserveAtLeast(m_len + inc);
			return *this;
		}

		VecCore<B>& ReserveAtLeast(sizet cap)
		{
			if (cap > Cap())
				ReserveAtLeast_NeedMore(cap);
			return *this;
		}

		VecCore<B>& ResizeAtLeast(sizet len)
		{
			if (len > m_len)
				AddN(len - m_len);
			else
				Resize_Shrink(len);
			return *this;
		}

		VecCore<B>& ResizeAtLeast(sizet len, Val const& x)
		{
			if (len > m_len)
				AddN(len - m_len, x);									// Container state is unchanged if this throws
			else
				Resize_Shrink(len);
			return *this;
		}

		VecCore<B>& ResizeExact(sizet len)
		{
			if (len > m_len)
			{
				ReserveExact(len);
				AddN(len - m_len);
			}
			else
				Resize_Shrink(len);
			return *this;
		}

		VecCore<B>& ResizeExact(sizet len, Val const& x)
		{
			if (len > m_len)
			{
				ReserveExact(len);
				AddN(len - m_len, x);									// Container state is unchanged if this throws
			}
			else
				Resize_Shrink(len);
			return *this;
		}

		VecCore<B>& ResizeInc(sizet len)               { EnsureThrow(len < SIZE_MAX - m_len); return ResizeAtLeast(m_len + len);    }
		VecCore<B>& ResizeInc(sizet len, Val const& x) { EnsureThrow(len < SIZE_MAX - m_len); return ResizeAtLeast(m_len + len, x); }

		VecCore<B>& ShrinkToFit()
		{
			if (!this->B::FixedCap() && Cap() > m_len)
			{
				VecCore<B> temp;
				temp.MergeFrom(*this);
				Swap(temp);
			}
			return *this;
		}

		Val& Add()
		{
			ReserveAtLeast(m_len + 1);
			if (std::is_trivially_constructible<Val>::value)
				Mem::Zero<Val>(Ptr() + m_len, 1);						// Ensure trivial object state is nulled out. Default constructor may not
			else
				new (Ptr() + m_len) Val;								// Container state is unchanged if this throws
			++m_len;
			return Ptr()[m_len - 1];
		}

		template <typename... Args>
		Val& Add(Args&&... args)
		{
			ReserveAtLeast(m_len + 1);
			new (Ptr() + m_len) Val(std::forward<Args>(args)...);		// Container state is unchanged if this throws
			++m_len;
			return Ptr()[m_len - 1];
		}

		VecCore<B>& AddN(sizet n)
		{
			if (n != 0)
			{
				EnsureThrow(n < SIZE_MAX - m_len);
				ReserveAtLeast(m_len + n);
				if (std::is_trivially_constructible<Val>::value)
					Mem::Zero<Val>(Ptr() + m_len, n);					// Ensure trivial object state is zeroed out. Default constructor may not
				else
				{
					static_assert(std::is_nothrow_default_constructible<Val>::value,
						"Requires a more complex implementation with try/catch to provide strong exception safety if default constructor can throw");
					for (sizet i=0; i!=n; ++i)
						new (Ptr() + m_len + i) Val;
				}
				m_len += n;
			}
			return *this;
		}

		VecCore<B>& AddN(sizet n, Val const& x)
		{
			if (n != 0)
			{
				EnsureThrow(n < SIZE_MAX - m_len);
				ReserveAtLeast(m_len + n);
				
				sizet i = 0;
				try
				{
					do
					{
						new (Ptr() + m_len + i) Val(x);
						++i;
					}
					while (i != n);
				}
				catch (...)
				{
					while (i)
					{
						--i;
						Ptr()[m_len + i].~Val();
					}
					throw;
				}
				
				m_len += n;
			}

			return *this;
		}

		VecCore<B>& AddSlice(Slice<Val> x)
		{
			sizet n = x.Len();
			if (n != 0)
			{
				EnsureThrow(n < SIZE_MAX - m_len);
				ReserveAtLeast(m_len + n);

				sizet i = 0;
				try
				{
					do
					{
						new (Ptr() + m_len + i) Val(x[i]);
						++i;
					}
					while (i != n);
				}
				catch (...)
				{
					while (i)
					{
						--i;
						Ptr()[m_len + i].~Val();
					}
					throw;
				}

				m_len += n;
			}

			return *this;
		}

		VecCore<B>& PopFirst() { return Erase(0, 1); }

		VecCore<B>& PopLast()
		{
			static_assert(std::is_nothrow_destructible<Val>::value, "Strong exception safety cannot be provided if destructor throws.");
			EnsureThrow(m_len > 0);
			--m_len;
			Ptr()[m_len].~Val();
			Mem::Zero<Val>(Ptr() + m_len, 1);
			return *this;
		}

		template <typename... Args>
		Val& Insert(sizet i, Args&&... args)
		{
			Val temp(std::forward<Args>(args)...);
			return Insert(i, std::move(temp));
		}

		Val& Insert(sizet i, Val&& x)
		{
			EnsureThrowWithNr(i <= m_len, i);
			ReserveAtLeast(m_len + 1);

			static_assert(std::is_nothrow_move_constructible<Val>::value, "Basic exception safety cannot be provided if move constructor throws.");
			static_assert(std::is_nothrow_destructible<Val>::value, "Basic exception safety cannot be provided if destructor throws.");

			for (sizet k=m_len; k!=i; --k)
			{
				new (Ptr() + k) Val(std::move(Ptr()[k-1]));
				Ptr()[k-1].~Val();
			}

			Val* pNew = Ptr() + i;
			new (pNew) Val(std::forward<Val>(x));
			++m_len;
			return *pNew;
		}

		VecCore<B>& InsertN(sizet i, sizet n)
		{
			EnsureThrowWithNr(i <= m_len, i);
			if (n != 0)
			{
				EnsureThrow(n < SIZE_MAX - m_len);
				ReserveAtLeast(m_len + n);
				Mem::Move<Val>(Ptr() + i + n, Ptr() + i, m_len - i);

				if (std::is_trivially_constructible<Val>::value)
					Mem::Zero<Val>(Ptr() + i, n);					// Ensure trivial object state is zeroed out. Default constructor may not
				else
				{
					static_assert(std::is_nothrow_default_constructible<Val>::value,
						"Requires a more complex implementation with try/catch to provide strong exception safety if default constructor can throw");
					for (sizet j=0; j!=n; ++j)
						new (Ptr() + i + j) Val;
				}
				m_len += n;
			}
			return *this;
		}

		VecCore<B>& Erase(sizet i, sizet n)
		{
			EnsureThrowWithNr(i <= m_len, i);
			EnsureThrowWithNr(n <= m_len - i, n);
			if (n)
			{
				static_assert(std::is_nothrow_move_constructible<Val>::value, "Basic exception safety cannot be provided if move constructor throws.");
				static_assert(std::is_nothrow_destructible<Val>::value, "Basic exception safety cannot be provided if destructor throws.");

				for (sizet k=i; k!=i+n; ++k)
					Ptr()[k].~Val();

				m_len -= n;

				for (sizet k=i; k!=m_len; ++k)
				{
					new (Ptr() + k) Val(std::move(Ptr()[k+n]));
					Ptr()[k+n].~Val();
				}

				Mem::Zero<Val>(Ptr() + m_len, n);
			}
			return *this;
		}

		VecCore<B>& SplitInto(sizet i, VecCore<B>& addTo)
		{
			if (i < m_len)
			{
				addTo.AddMoveRange(Ptr() + i, m_len - i);
				m_len = i;
			}
			else
				EnsureThrow(i == m_len);

			return *this;
		}

		VecCore<B>& MergeFrom(VecCore<B>& from)
		{
			AddMoveRange(from.Ptr(), from.m_len);
			from.m_len = 0;
			return *this;
		}

	private:
		__declspec(noinline) void ReserveAtLeast_NeedMore(sizet cap)
		{
			if (this->B::FixedCap())
				this->B::ReserveExact(cap);
			else
			{
				enum { MinCap = 16/sizeof(Val) };
				if (cap < MinCap)
					cap = MinCap;
				if (cap < 2 * Cap())
					cap = 2 * Cap();
				this->B::ReserveExact(cap);
			}
		}

		void Resize_Shrink(sizet len)
		{
			if (m_len > len)
			{
				if (std::is_trivially_destructible<Val>::value)
				{
					Mem::Zero<Val>(Ptr() + len, m_len - len);
					m_len = len;
				}
				else
				{
					do PopLast();										// Does not require nothrow_move_constructible, unlike Erase
					while (m_len > len);
				}
			}
		}

		void AddMoveRange(Val* p, sizet n)
		{
			ReserveExact(m_len + n);

			static_assert(std::is_nothrow_move_constructible<Val>::value, "Basic exception safety cannot be provided if move constructor throws.");
			static_assert(std::is_nothrow_destructible<Val>::value, "Basic exception safety cannot be provided if destructor throws.");

			while (n--)
			{
				new (Ptr() + m_len) Val(std::move(*p));
				p->~Val();
				++m_len;
				++p;
			}
		}
	};


	template <class T>         struct Vec    : VecCore<VecBaseHeap<T>>    { Vec   () = default; Vec   (sizet i) { ResizeExact(i); } };
	template <class T, uint C> struct VecFix : VecCore<VecBaseFixed<T,C>> { VecFix() = default; VecFix(sizet i) { ResizeExact(i); } };

}
