#include "AtIncludes.h"
#include "AtNum.h"

namespace At
{
	bool IsPowerOf2(uint64 n)
	{
		if (!n)
			return false;

		while (true)
		{
			uint64 shifted = (n >> 1);
			if ((n & 1) == 1)
				return shifted == 0;
			n = shifted;
		}
	}


	template <typename T> unsigned int NrBitsUsed(T value)
	{
		if (!value)
			return 0;

		// Find the highest set bit using bisection
		unsigned int l = 0;
		unsigned int h = 8 * sizeof(T);
		while (h - l > 1)
		{
			unsigned int t = (l + h) / 2;
			if (value >> t)
				l = t;
			else
				h = t;
		}

		return h;
	}

	template unsigned int NrBitsUsed<uint32>(uint32);

}
