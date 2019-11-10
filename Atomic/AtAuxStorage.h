#pragma once

#include "AtStr.h"


namespace At
{

	// AuxStorage

	// Used in the context of e.g. encoding or decoding a message that might require many auxiliary allocations.
	// Use of AuxStorage allows efficient reuse of a number of small Str objects throughout the encoding/decoding process.

	class AuxStorage : NoCopy
	{
	private:
		Vec<Str> m_unused;

		friend class AuxStr;
	};

	
	class AuxStr : public Str
	{
	public:
		AuxStr(AuxStorage& storage) : m_storage(storage)
		{
			if (m_storage.m_unused.Any())
			{
				// Get unused Str from storage
				Swap(m_storage.m_unused.Last());
				m_storage.m_unused.PopLast();
			}
		}

		~AuxStr()
		{
			// Clear, but do not free; then place unused Str into storage
			Clear();								
			m_storage.m_unused.Add().Swap(*this);
		}

	private:
		AuxStorage& m_storage;
	};

}
