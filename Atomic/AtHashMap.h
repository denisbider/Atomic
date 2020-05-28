#pragma once

#include "AtHeap.h"


namespace At
{

	// Contained type T must have methods:
	// - KeyType Key() const;
	// - static uint64 HashOfKey(KeyType) const;

	template <class T>
	class HashMap
	{
	public:
		using KeyOrRef = decltype(((T const*) nullptr)->Key());
		using Key      = std::remove_const_t<std::remove_reference_t<KeyOrRef>>;

	public:
		HashMap<T>& SetNrBuckets(sizet nrBuckets) { EnsureThrow(m_v.Len() == 0); m_v.Resize(nrBuckets); return *this; }
		sizet NrBuckets() const { return m_v.Len(); }

		T& Add(T const& x) { T c{x}; return Add(c.Key(), std::move(c)); }
		T& Add(T&&      x) {         return Add(x.Key(), std::move(x)); }

		template <typename... Args>
		T& Add(Args&&... args)
		{
			Entry& e = m_heap.Add();
			e.m_x.Init(std::forward<Args>(args)...);
			Entry*& bucketEntryPtr = GetBucketEntryPtr(e.m_x->Key());
			e.m_next = bucketEntryPtr;
			bucketEntryPtr = &e;
			return e.m_x.Ref();
		}

		T* Find(KeyOrRef k)
		{
			Entry* e = GetBucketEntryPtr(k);
			while (e)
				if (e->m_x->Key() != k)
					e = e->m_next;
				else
					return e->m_x.Ptr();
			
			return nullptr;
		}

		bool Erase(KeyOrRef k)
		{
			Entry** pPrev = &GetBucketEntryPtr(k);
			Entry* e = *pPrev;

			while (e)
				if (e->m_x->Key() != k)
				{
					pPrev = &(e->m_next);
					e = e->m_next;
				}
				else
				{
					*pPrev = e->m_next;
					m_heap.Erase(*e);
					return true;
				}
			
			return false;
		}

	private:
		using Entry = typename Heap<T>::Entry;

		Heap<T>     m_heap;
		Vec<Entry*> m_v;

		Entry*& GetBucketEntryPtr(KeyOrRef k)
		{
			sizet nrBuckets = NrBuckets();
			EnsureThrow(nrBuckets);

			sizet bucketIndex = (sizet) (T::HashOfKey(k) % nrBuckets);
			return m_v[bucketIndex];
		}
	};

}
