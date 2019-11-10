#pragma once

#include "AtMutex.h"
#include "AtSocket.h"
#include "AtTime.h"


namespace At
{

	// An IP-based multi-level load throttle such that:
	// - An attacker who can use only a single IPv4 or IPv6 address can tie up only a small, limited fraction of the server's resources.
	// - An attacker who can vary 1 of 4 parts of their IP address (e.g. lowest byte IPv4, lowest 32 bits IPv6) can tie up ~25% of the server's resources.
	// - An attacker who can vary 2 of 4 parts of their IP address (e.g. lowest 2 bytes IPv4, lowest 64 bits IPv6) can tie up ~50% of the server's resources.
	// - An attacker who can vary 3 of 4 parts of their IP address can tie up ~75% of the server's resources.
	// - An attacker has to be able to vary all parts of their IP address in order to tie up 100% of the server's resources.

	class IpFairThrottle
	{
	public:
		// Sets the maximum number of simultaneous holds that can ever be held by a single IP address.
		// Must be set before first use. Must be at least 4, and must be divisible by 4.
		// There are 4 levels, and we do not support less than 1 acquire per level, or a varying max number of holds per level.
		void SetMaxHoldsBySingleIp(sizet n)
		{
			EnsureThrow(!m_inited);
			EnsureThrow((n >= 4) && ((n % 4) == 0));	// A single IP can always have at least 4 holds, one per each of the 4 levels
			m_maxHoldsBySingleIp = n;
		}

		// Sets the maximum number of simultaneous holds that can ever be held globally by all clients.
		// Must be set before first use. Must be a multiple of the value set using SetMaxHoldsBySingleIp.
		void SetMaxHoldsGlobal(sizet n)
		{
			EnsureThrow(!m_inited);
			EnsureThrow((n >= 8) && ((n % 4) == 0));	// Does not make much sense to have less than 2 buckets per level
			m_maxHoldsGlobal = n;
		}

		struct HoldLocator
		{
			sizet  m_levelIndex;
			sizet  m_bucketIndex;
			uint64 m_holdId;
		};

		// Attempts to acquire a hold based on the provided IP. Returns true if successful, false otherwise.
		bool TryAcquireHold(SockAddr const& sa, Time durationUntilExpire, HoldLocator& hl);
		bool TryAcquireHold(SockAddr const& sa, HoldLocator& hl) { return TryAcquireHold(sa, Time::Max(), hl); }

		// Releases a hold. Returns true if the hold was found and released, false if not found (e.g. because it expired).
		bool ReleaseHold(HoldLocator hl);

	private:
		struct Hold
		{
			uint64 m_id;
			Time   m_timeExpires;
		};

		struct Bucket
		{
			Vec<Hold> m_holds;
		};

		struct Level
		{
			Vec<Bucket> m_buckets;
		};

		// Set by user before init
		sizet  m_maxHoldsBySingleIp {};
		sizet  m_maxHoldsGlobal {};

		// Set on init
		bool   m_inited {};
		sizet  m_nrBucketsPerLevel;
		sizet  m_maxHoldsPerBucket;
		uint64 m_perLevelHashSeeds[4];

		Mutex  m_mx;
		uint64 m_nextHoldId {};
		Level  m_levels[4];

		void CheckInit() { if (!m_inited) Init(); }
		void Init();
		bool TryAcquireHold_Inner(sizet levelIndex, SockAddr const& sa, Time durationUntilExpire, HoldLocator& hl);
		uint32 Ip4Hash(sizet levelIndex, uint32 ip4);
		uint32 Ip6Hash(sizet levelIndex, uint64 ip6hi);
	};

}
