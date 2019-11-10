#pragma once

#include "AtMap.h"
#include "AtPinStore.h"

namespace At
{
	struct NameValuePair
	{
		NameValuePair(Seq n, Seq v) : m_name(n), m_value(v) {}
		Seq Key() const { return m_name; }

		Seq m_name;
		Seq m_value;
	};

	template <class B>
	class NameValuePairsBase : public MapCore<B>
	{
	public:
		bool Contains(Seq name) const noexcept { return MapCore<B>::Find(name).Any(); }

		Seq Get(Seq name) const
		{
			MapCore<B>::ConstIt it { MapCore<B>::Find(name) };
			if (it.Any())
				return it->m_value;
			return Seq();
		}

		struct ValueIt : MapCore<B>::ConstIt
		{
			ValueIt(MapCore<B>::ConstIt const& x) : MapCore<B>::ConstIt(x) {}

			Seq        operator*  () const { return  MapCore<B>::ConstIt::operator* ().m_value; }
			Seq const* operator-> () const { return &MapCore<B>::ConstIt::operator->().m_value; }
		};

		Range<ValueIt> EqualRange(Seq name) const { return MapCore<B>::EqualRange(name); }
	};

	template <class B>
	class NameValuePairs : public NameValuePairsBase<B>
	{
	public:
		NameValuePairs& Add(Seq name, Seq value)
		{
			NameValuePair v { name, value };
			MapCore<B>::Add(std::move(v));
			return *this;
		}
	};

	template <class B>
	class NameValuePairsWithStore : public NameValuePairsBase<B>
	{
	public:
		NameValuePairsWithStore(sizet bytesPerPage = 4000) : m_store(bytesPerPage) {}

		NameValuePairsWithStore& Add(Seq name, Seq value)
		{
			Seq nameInStore { m_store.AddStr(name) };
			Seq valueInStore { m_store.AddStr(value) };
			NameValuePair v { nameInStore, valueInStore };
			MapCore<B>::Add(std::move(v));
			return *this;
		}

	private:
		PinStore m_store;
	};

	using ExactNameValuePairsBase            = NameValuePairsBase     <MapCritLess           <KeyInVal<NameValuePair>>>;
	using InsensitiveNameValuePairsBase      = NameValuePairsBase     <MapCritLessInsensitive<KeyInVal<NameValuePair>>>;
	using ExactNameValuePairs                = NameValuePairs         <MapCritLess           <KeyInVal<NameValuePair>>>;
	using InsensitiveNameValuePairs          = NameValuePairs         <MapCritLessInsensitive<KeyInVal<NameValuePair>>>;
	using ExactNameValuePairsWithStore       = NameValuePairsWithStore<MapCritLess           <KeyInVal<NameValuePair>>>;
	using InsensitiveNameValuePairsWithStore = NameValuePairsWithStore<MapCritLessInsensitive<KeyInVal<NameValuePair>>>;
}
