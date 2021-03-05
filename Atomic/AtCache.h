#pragma once

#include "AtIncludes.h"
#include "AtTime.h"


namespace At
{

	template <class Key, class Value>
	class Cache
	{
	private:
		template <class Value>
		struct Entry
		{
			Value* m_value;
			Time m_lastAccessTime;		// MUST be obtained using Time::StrictNow, as opposed to NonStrictNow, to ensure uniqueness
		};

	public:
		~Cache() noexcept { Clear(); }


		void Clear() noexcept
		{
			static_assert(std::is_nothrow_destructible<Value>::value, "Cannot provide exception safety if destructor can throw");

			EntriesByKey::iterator it = m_entriesByKey.begin();
			while (it != m_entriesByKey.end())
			{
				if (it->second.m_value != nullptr)
				{
					delete it->second.m_value;
					it->second.m_value = nullptr;
				}

				++it;
			}

			m_entriesByKey.clear();
			m_keysByLastAccessTime.clear();
		}


		Value* FindEntry(Key const& key)
		{
			EntriesByKey::iterator it = m_entriesByKey.find(key);
			if (it == m_entriesByKey.end())
				return nullptr;

			Time prevTime = it->second.m_lastAccessTime;
			it->second.m_lastAccessTime = Time::StrictNow();

			KeysByLastAccessTime::iterator timeIt = m_keysByLastAccessTime.find(prevTime);
			EnsureThrow(timeIt != m_keysByLastAccessTime.end());
			m_keysByLastAccessTime.erase(timeIt);
			m_keysByLastAccessTime.insert(std::make_pair(it->second.m_lastAccessTime, key));
			return it->second.m_value;
		}

	
		Value& FindOrInsertEntry(Key const& key)
		{
			EntriesByKey::iterator it = m_entriesByKey.find(key);
			if (it != m_entriesByKey.end())
			{
				Time prevTime = it->second.m_lastAccessTime;
				it->second.m_lastAccessTime = Time::StrictNow();

				KeysByLastAccessTime::iterator timeIt = m_keysByLastAccessTime.find(prevTime);
				EnsureThrow(timeIt != m_keysByLastAccessTime.end());
				m_keysByLastAccessTime.erase(timeIt);
			}
			else
			{
				it = m_entriesByKey.insert(std::make_pair(key, Entry<Value>())).first;
				it->second.m_value = new Value;
				it->second.m_lastAccessTime = Time::StrictNow();
			}

			m_keysByLastAccessTime.insert(std::make_pair(it->second.m_lastAccessTime, key));
			return *(it->second.m_value);
		}


		void RemoveEntries(Key const& firstKey, Key const& lastKey)
		{
			EnsureThrow(firstKey < lastKey || firstKey == lastKey);

			EntriesByKey::iterator itStart = m_entriesByKey.lower_bound(firstKey);
			EntriesByKey::iterator itEnd = m_entriesByKey.upper_bound(lastKey);

			if (itEnd != itStart)
			{		
				EntriesByKey::iterator it = itStart;
				while (it != itEnd)
				{
					m_keysByLastAccessTime.erase(it->second.m_lastAccessTime);
				
					delete it->second.m_value;
					it->second.m_value = nullptr;
				
					++it;
				}
		
				m_entriesByKey.erase(itStart, itEnd);
			}
		}


		void PruneEntries(sizet targetSize, Time maxAge)
		{
			Time now = Time::StrictNow();
			while (true)
			{
				KeysByLastAccessTime::iterator timeIt = m_keysByLastAccessTime.begin();
				if (timeIt == m_keysByLastAccessTime.end())
					break;

				if (m_entriesByKey.size() < targetSize)
					if ((now - timeIt->first) <= maxAge)
						break;

				EntriesByKey::iterator it = m_entriesByKey.find(timeIt->second);
				EnsureThrow(it != m_entriesByKey.end());
				if (it->second.m_value != nullptr)
				{
					delete it->second.m_value;
					it->second.m_value = nullptr;
				}

				m_entriesByKey.erase(it);
				m_keysByLastAccessTime.erase(timeIt);
			}
		}


	private:
		typedef std::map<Key, Entry<Value>> EntriesByKey;
		typedef std::map<Time, Key> KeysByLastAccessTime;
	
		EntriesByKey m_entriesByKey;
		KeysByLastAccessTime m_keysByLastAccessTime;	
	};

}
