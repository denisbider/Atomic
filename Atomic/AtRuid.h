#pragma once

#include "AtIncludes.h"

namespace At
{
	// Run-time Unique ID

	class Ruid : NoCopy
	{
	public:
		Ruid(char const* tag, Ruid const* parent) noexcept : m_tag(tag), m_parent(parent) {}

		char const* Tag() const noexcept { return m_tag; }

		bool Is(Ruid const& x) const noexcept { return (this == &x) || (m_parent && m_parent->Is(x)); }
		bool Equal(Ruid const& x) const noexcept { return this == &x; }

	private:
		char const* m_tag;
		Ruid const* m_parent;
	};

	#define DECL_RUID(TAG)           extern Ruid id_##TAG;
	#define DEF_RUID_B(TAG)          Ruid id_##TAG { #TAG, nullptr };
	#define DEF_RUID_X(TAG, PARENT)  Ruid id_##TAG { #TAG, &(PARENT) };
}
