#pragma once

#include "AtRp.h"
#include "AtVec.h"


namespace At
{

	template <class T>
	class RpVec : public Vec<Rp<T>>
	{
	public:
		T*       FindInnerPtr(std::function<bool (Val&       x)> criterion)       { sizet i = FindIdx(criterion); if (i == SIZE_MAX) return nullptr; return Ptr()[i].Ptr(); }
		T const* FindInnerPtr(std::function<bool (Val const& x)> criterion) const { sizet i = FindIdx(criterion); if (i == SIZE_MAX) return nullptr; return Ptr()[i].Ptr(); }
	};

}
