#pragma once

#include "AtStr.h"


namespace At
{

	class BitFlagDescriber
	{
	public:
		BitFlagDescriber(uint64 allBits) { m_allBits = allBits; m_describedBits = 0; m_desc.Clear().ReserveExact(100); }

		void DescribeBit(uint64 bit, Seq bitName)
		{
			if ((m_allBits & bit) == bit)
			{
				m_desc.IfAny(", ").Add(bitName);
				m_describedBits |= bit;
			}
		}

		Str&& Final()
		{
			if (m_describedBits != m_allBits)
			{
				uint64 unrecognizedBits = (m_allBits & ~m_describedBits);
				m_desc.IfAny(", ").UInt(unrecognizedBits);
			}

			if (!m_desc.Any())
				m_desc.Set("(none)");

			return std::move(m_desc);
		}

	private:
		uint64 m_allBits;
		uint64 m_describedBits;
		Str    m_desc;
	};

}

