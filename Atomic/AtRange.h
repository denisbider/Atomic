#pragma once

namespace
{
	template <class It>
	class Range
	{
	public:
		Range(It first, It last) : m_first(first), m_last(last) {}

		template <class ItOther>
		Range(Range<ItOther> const& x) : m_first(x.m_first), m_last(x.m_last) {}

		It begin() { return m_first; }
		It end  () { return m_last; }

		It m_first;
		It m_last;
	};
}
