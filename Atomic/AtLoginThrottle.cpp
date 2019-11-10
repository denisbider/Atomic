#include "AtIncludes.h"
#include "AtLoginThrottle.h"

#include "AtTime.h"

namespace At
{
	LoginThrottle::~LoginThrottle()
	{
		for (LoginFailure*& entry : m_entries)
			if (entry != nullptr)
			{
				delete entry;
				entry = nullptr;
			}
	}


	bool LoginThrottle::HaveRecentFailure(Seq userName, Seq remoteIdAddr)
	{
		Locker locker(m_mx);

		Time now = Time::StrictNow();
		Time cutOff = now - Time::FromSeconds(ThrottleSeconds);

		sizet i = m_nextIndex;
		while (true)
		{
			if (i == 0)
				i = m_entries.Len() - 1;
			else
				--i;

			LoginFailure* entry = m_entries[i];

			// Reached empty part of list without entries?
			if (!entry)
				return false;

			// Reached cut off?
			if (entry->time < cutOff)
				return false;

			// Match?
			if (userName.EqualInsensitive(entry->userName) ||
				remoteIdAddr.EqualInsensitive(entry->remoteIdAddr))
				return true;

			// Came all the way around?
			if (i == m_nextIndex)
				return false;
		}
	}


	void LoginThrottle::AddLoginFailure(Seq userName, Seq remoteIdAddr)
	{
		Locker locker(m_mx);

		Time now = Time::StrictNow();
		Time cutOff = now - Time::FromSeconds(ThrottleSeconds);

		// Make room for entry
		LoginFailure* entry = m_entries[m_nextIndex];

		if (entry)
		{
			if (entry->time > cutOff)
			{
				sizet origSize = m_entries.Len();
				m_entries.Resize(origSize * 2, nullptr);
				memmove(&m_entries[m_nextIndex], &m_entries[m_nextIndex+origSize], (origSize - m_nextIndex) * sizeof(LoginFailure));
				memset(&m_entries[m_nextIndex], 0, origSize * sizeof(LoginFailure));
				entry = nullptr;
			}
		}

		if (!entry)
		{
			m_entries[m_nextIndex] = new LoginFailure();
			entry = m_entries[m_nextIndex];
		}
	
		// Fill out entry
		entry->time = now;
		entry->userName = userName;
		entry->remoteIdAddr = remoteIdAddr;

		// Move m_nextIndex
		if (++m_nextIndex == m_entries.Len())
			m_nextIndex = 0;
	}
}
