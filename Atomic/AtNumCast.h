#pragma once

#include "AtIncludes.h"

namespace At
{
	// Signed -> Signed

	template <class OutType, class InType,
		std::enable_if_t<(
				std::numeric_limits<InType >::is_signed &&
				std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) > sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is larger
		if (value < std::numeric_limits<OutType>::min()) throw OutOfBounds();
		if (value > std::numeric_limits<OutType>::max()) throw OutOfBounds();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				std::numeric_limits<InType >::is_signed &&
				std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is NOT larger
		return (OutType) value;
	}


	// Signed -> Unsigned

	template <class OutType, class InType,
		std::enable_if_t<(
				 std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) > sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is larger
		if (value < 0) throw OutOfBounds();
		if (value > std::numeric_limits<OutType>::max()) throw OutOfBounds();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				 std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is NOT larger
		if (value < 0) throw OutOfBounds();
		return (OutType) value;
	}


	// Unsigned -> Signed

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				 std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) >= sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is NOT smaller
		if (value > (InType) std::numeric_limits<OutType>::max()) throw OutOfBounds();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				 std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) < sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is smaller
		return (OutType) value;
	}


	// Unsigned -> Unsigned

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) > sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is larger
		if (value > std::numeric_limits<OutType>::max()) throw OutOfBounds();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType NumCast(InType value)
	{
		// Input type is NOT larger
		return (OutType) value;
	}
}
