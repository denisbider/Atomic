#pragma once

#include "AtNum.h"
#include "AtVec.h"
#include "AtOpt.h"


namespace At
{

	// An in-memory object storage mechanism. Strengths:
	// + efficient object addition and removal
	// + efficient for small and large numbers of objects
	//
	// Weaknesses:
	// - no indexable access or enumeration
	// - does not release any allocated storage until end of lifetime for the whole Heap

	template <class T>
	class Heap
	{
	public:
		static_assert(std::is_nothrow_destructible<T>::value, "Throwing destructors not supported");

		struct Entry
		{
			Entry* m_next {};	// When Entry is in use, free for use by the user. When Entry has been erased, points to next free Entry
			Opt<T> m_x;
		};

	private:
		static sizet constexpr EntriesPerChunk = PickMax<sizet>(7, (4096U - (8*sizeof(void*))) / sizeof(Entry));
		using Entries = VecFix<Entry, EntriesPerChunk>;
		
		struct Chunk
		{
			Chunk*  m_prevChunk {};
			Entries m_entries;

			Chunk(Chunk* prevChunk) : m_prevChunk(prevChunk) {}
			~Chunk() noexcept { delete m_prevChunk; }
		};

	public:
		~Heap() noexcept { delete m_lastChunk; }

		// Constructs a new Entry in the Heap, or returns a previously Erased one. The Entry will remain in place as long as the Heap exists.
		Entry& Add()
		{
			if (m_lastFreeEntry)
			{
				Entry* e = m_lastFreeEntry;
				EnsureThrow(!e->m_x.Any());
				m_lastFreeEntry = e->m_next;
				e->m_next = nullptr;
				return *e;
			}

			if (!m_lastChunk || m_lastChunk->m_entries.Len() == EntriesPerChunk)
				m_lastChunk = new Chunk(m_lastChunk);

			return m_lastChunk->m_entries.Add();
		}

		// Clears the value stored in the Entry and marks the Entry available for reuse.
		void Erase(Entry& e)
		{
			e.m_x.Clear();
			e.m_next = m_lastFreeEntry;
			m_lastFreeEntry = &e;
		}

	private:
		Chunk* m_lastChunk {};
		Entry* m_lastFreeEntry {};
	};

}
