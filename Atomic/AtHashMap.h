#pragma once

#include "AtVec.h"


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
		T& Add(KeyOrRef k, Args&&... args)
		{
			sizet nrBuckets { NrBuckets() };
			EnsureThrow(nrBuckets);

			sizet bucketIndex { NumCast<sizet>(T::HashOfKey(k) % nrBuckets) };
			return m_v[bucketIndex].Add(std::forward<Args>(args)...);
		}

		T* Find(KeyOrRef k)
		{
			sizet nrBuckets { NrBuckets() };
			EnsureThrow(nrBuckets);

			sizet bucketIndex { NumCast<sizet>(T::HashOfKey(k) % nrBuckets) };
			Vec<T>& v2 { m_v[bucketIndex] };
			for (T& x : v2)
				if (x.Key() == k)
					return &x;

			return nullptr;
		}

		bool Erase(KeyOrRef k)
		{
			sizet nrBuckets { NrBuckets() };
			EnsureThrow(nrBuckets);

			sizet bucketIndex { NumCast<sizet>(T::HashOfKey(k) % nrBuckets) };
			Vec<T>& v2 { m_v[bucketIndex] };
			for (sizet i=0; i!=v2.Len(); ++i)
				if (v2[i].Key() == k)
				{
					v2.Erase(i, 1);
					return true;
				}
			
			return false;
		}

	private:
		Vec<Vec<T>> m_v;
	};

}
