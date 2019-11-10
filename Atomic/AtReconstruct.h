#pragma once

#include "AtEnsure.h"

namespace At
{

	template <class T, typename... Args> void Reconstruct(T* p, Args&&... args)
	{
		static_assert(std::is_nothrow_destructible<T>::value, "If destructor throws, cannot provide strong exception safety");
		static_assert(std::is_nothrow_move_constructible<T>::value, "If move constructor throws, cannot provide basic exception safety");
		EnsureAbort(typeid(*p) == typeid(T));		// If object is polymorphic, ensure that we have the most derived type

		// In addition to exception safety, below is also safe to move-construct related objects (e.g. child into parent)
		T temp(std::forward<Args>(args)...);		// New object constructor may throw. After, destructor cannot throw
		p->~T();									// Cannot throw
		new (p) T(std::move(temp));					// Cannot throw
	}
	
	template <class T, typename... Args> inline void Reconstruct(T& x, Args&&... args)
		{ Reconstruct(&x, std::forward<Args>(args)...); }

}
