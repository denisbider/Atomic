#pragma once

#include "AtIncludes.h"

namespace At
{
	// Largely identical to NumCast, but coerces input value to output range instead of throwing.

	// Signed -> Signed

	template <class OutType, class InType,
		std::enable_if_t<(
				std::numeric_limits<InType >::is_signed &&
				std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) > sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is larger
		if (value < std::numeric_limits<OutType>::min()) return std::numeric_limits<OutType>::min();
		if (value > std::numeric_limits<OutType>::max()) return std::numeric_limits<OutType>::max();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				std::numeric_limits<InType >::is_signed &&
				std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
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
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is larger
		if (value < 0) return 0;
		if (value > std::numeric_limits<OutType>::max()) return std::numeric_limits<OutType>::max();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				 std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is NOT larger
		if (value < 0) return 0;
		return (OutType) value;
	}


	// Unsigned -> Signed

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				 std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) >= sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is NOT smaller
		if (value > (InType) std::numeric_limits<OutType>::max()) return std::numeric_limits<OutType>::max();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				 std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) < sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
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
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is larger
		if (value > std::numeric_limits<OutType>::max()) return std::numeric_limits<OutType>::max();
		return (OutType) value;
	}

	template <class OutType, class InType,
		std::enable_if_t<(
				!std::numeric_limits<InType >::is_signed &&
				!std::numeric_limits<OutType>::is_signed &&
				sizeof(InType) <= sizeof(OutType)
			), int> = 0>
	inline OutType SatCast(InType value) noexcept
	{
		// Input type is NOT larger
		return (OutType) value;
	}
}
