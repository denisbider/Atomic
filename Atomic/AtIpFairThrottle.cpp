#include "AtIncludes.h"
#include "AtIpFairThrottle.h"

#include "AtCrypt.h"


namespace At
{

	void IpFairThrottle::Init()
	{
		EnsureThrow(!m_inited);
		EnsureThrow( m_maxHoldsBySingleIp       >= 4);
		EnsureThrow((m_maxHoldsBySingleIp %  4) == 0);
		EnsureThrow( m_maxHoldsGlobal           >= 8);
		EnsureThrow((m_maxHoldsGlobal     %  4) == 0);
		EnsureThrow( m_maxHoldsGlobal           >= 2 * m_maxHoldsBySingleIp);

		// Calculate bucket vector dimensions
		m_maxHoldsPerBucket = m_maxHoldsBySingleIp / 4;

		sizet maxHoldsPerLevel { m_maxHoldsGlobal / 4 };
		m_nrBucketsPerLevel = maxHoldsPerLevel / m_maxHoldsPerBucket;

		// Verify that the actual effective global maximum is not less than expected due to rounding in integer division.
		EnsureThrow(m_maxHoldsGlobal == m_maxHoldsPerBucket * m_nrBucketsPerLevel * 4);
		
		// Generate level-specific hash seeds. This makes IP-to-bucket mappings more unique, and collisions less predictable to attackers.
		static_assert(sizeof(m_perLevelHashSeeds) == 4*sizeof(uint64), "Unexpected");
		Crypt::GenRandom(m_perLevelHashSeeds, sizeof(m_perLevelHashSeeds));

		// Initialize bucket vectors
		for (sizet iLevel=0; iLevel!=4; ++iLevel)
		{
			Level& level { m_levels[iLevel] };
			level.m_buckets.ResizeExact(m_nrBucketsPerLevel);
			for (Bucket& bucket : level.m_buckets)
				bucket.m_holds.FixCap(m_maxHoldsPerBucket);
		}

		m_inited = true;
	}


	bool IpFairThrottle::TryAcquireHold(SockAddr const& sa, Time durationUntilExpire, HoldLocator& hl)
	{
		Locker locker { m_mx };
		CheckInit();

		bool          success = TryAcquireHold_Inner(3, sa, durationUntilExpire, hl);
		if (!success) success = TryAcquireHold_Inner(2, sa, durationUntilExpire, hl);
		if (!success) success = TryAcquireHold_Inner(1, sa, durationUntilExpire, hl);
		if (!success) success = TryAcquireHold_Inner(0, sa, durationUntilExpire, hl);
		return success;
	}


	bool IpFairThrottle::TryAcquireHold_Inner(sizet levelIndex, SockAddr const& sa, Time durationUntilExpire, HoldLocator& hl)
	{
		uint32 ipHash {};

		     if (sa.IsIp4() || sa.IsIp4MappedIp6()) ipHash = Ip4Hash(levelIndex, sa.GetIp4Nr());
		else if (sa.IsIp6())                        ipHash = Ip6Hash(levelIndex, sa.GetIp6Hi());
		else EnsureThrow(!"Unexpected address type");

		Level&  level       { m_levels[levelIndex] };
		sizet   bucketIndex { ipHash % level.m_buckets.Len() };
		Bucket& bucket      { level.m_buckets[bucketIndex] };

		Time now { Time::Min() };
		if (bucket.m_holds.Len() >= m_maxHoldsPerBucket)
		{
			// Clear any expired holds
			now = Time::StrictNow();
			for (sizet i=0; i!=bucket.m_holds.Len(); )
				if (now >= bucket.m_holds[i].m_timeExpires)
					bucket.m_holds.Erase(i, 1);
				else
					++i;

			if (bucket.m_holds.Len() >= m_maxHoldsPerBucket)
				return false;
		}

		Hold& hold { bucket.m_holds.Add() };
		hold.m_id = m_nextHoldId++;
		
		if (durationUntilExpire == Time::Max())
			hold.m_timeExpires = Time::Max();
		else
		{
			if (now == Time::Min())
				now = Time::StrictNow();

			hold.m_timeExpires = now + durationUntilExpire;
		}

		hl.m_levelIndex  = levelIndex;
		hl.m_bucketIndex = bucketIndex;
		hl.m_holdId      = hold.m_id;
		return true;
	}


	uint32 IpFairThrottle::Ip4Hash(sizet levelIndex, uint32 ip4)
	{
		     if (levelIndex == 0) ip4 &= 0xFF000000;
		else if (levelIndex == 1) ip4 &= 0xFFFF0000;
		else if (levelIndex == 2) ip4 &= 0xFFFFFF00;
		else EnsureThrow(levelIndex == 3);

		uint32 const FnvPrime32 = 16777619;
		uint32 x = ((uint32) m_perLevelHashSeeds[levelIndex]) ^ ip4;
		x = ((x >> 16) ^ x) * FnvPrime32;
		x = ((x >> 16) ^ x) * FnvPrime32;
		x = ((x >> 16) ^ x);
		return x;
	}


	uint32 IpFairThrottle::Ip6Hash(sizet levelIndex, uint64 ip6hi)
	{
		     if (levelIndex == 0) ip6hi &= 0xFFFF000000000000ull;
		else if (levelIndex == 1) ip6hi &= 0xFFFFFFFF00000000ull;
		else if (levelIndex == 2) ip6hi &= 0xFFFFFFFFFFFF0000ull;
		else EnsureThrow(levelIndex == 3);

		uint64 const FnvPrime64 = 1099511628211ull;
		uint64 x = m_perLevelHashSeeds[levelIndex] ^ ip6hi;
		x = ((x >> 32) ^ x) * FnvPrime64;
		x = ((x >> 32) ^ x) * FnvPrime64;
		x = ((x >> 32) ^ x);
		return (uint32) x;
	}


	bool IpFairThrottle::ReleaseHold(HoldLocator hl)
	{
		Locker locker { m_mx };
		EnsureThrow(m_inited);

		EnsureThrow(hl.m_levelIndex < 4);
		Level&  level  = m_levels[hl.m_levelIndex];
		Bucket& bucket = level.m_buckets[hl.m_bucketIndex];

		for (sizet i=0; i!=bucket.m_holds.Len(); ++i)
			if (bucket.m_holds[i].m_id == hl.m_holdId)
			{
				bucket.m_holds.Erase(i, 1);
				return true;
			}

		return false;
	}

}
