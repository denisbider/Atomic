#pragma once

#include "AtMutex.h"
#include "AtStr.h"
#include "AtTime.h"
#include "AtVec.h"

namespace At
{
	class LoginThrottle : NoCopy
	{
	public:
		enum { ThrottleSeconds = 3 };

		LoginThrottle() { m_entries.ResizeAtLeast(1, nullptr); }
		~LoginThrottle();

		// Returns true if there was a recent login failure either for the specified user name, or from the specified address.
		bool HaveRecentFailure(Seq userName, Seq remoteIdAddr);

		// Registers a login failure that used the specified user name and came from the specified address.
		void AddLoginFailure(Seq userName, Seq remoteIdAddr);

	private:
		struct LoginFailure
		{
			Time time;
			Str	userName;
			Str	remoteIdAddr;
		};

		Mutex m_mx;
		Vec<LoginFailure*> m_entries;
		sizet m_nextIndex {};
	};
}
