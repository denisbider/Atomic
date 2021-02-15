#include "AtIncludes.h"
#include "AtStrList.h"

#include "AtCrypt.h"
#include "AtHashMap.h"
#include "AtInitOnFirstUse.h"


namespace At
{

	Vec<Seq> VecStrToSeq(Vec<Str> const& x)
	{
		Vec<Seq> v;
		v.ReserveExact(x.Len());
		for (Seq s : x)
			v.Add(s);
		return v;
	}


	Vec<Str> VecSeqToStr(Vec<Seq> const& x)
	{
		Vec<Str> v;
		v.ReserveExact(x.Len());
		for (Seq s : x)
			v.Add(s);
		return v;
	}


	namespace Internal
	{
		LONG volatile g_strListDiff_initFlag {};
		uint64 g_strListDiff_fnvHashSeed {};
	}

	using namespace Internal;


	void StrListCompare(Vec<Seq> const& oldEntries, Vec<Seq> const& newEntries, Vec<Seq>& entriesRemoved, Vec<Seq>& entriesAdded)
	{
		entriesRemoved.Clear();
		entriesAdded.Clear();

		// This HashMap approach has significant setup costs when comparing small lists,
		// but improves worst-case performance from O(N*M) to O(N+M) when comparing large lists

		InitOnFirstUse(&g_strListDiff_initFlag, [] ()
			{ g_strListDiff_fnvHashSeed = Crypt::GenRandomNr(UINT64_MAX); } );

		struct PresenceEntry
		{
			Seq    m_s;

			PresenceEntry(Seq s) : m_s(s) {}

			static uint64 HashOfKey(Seq s) { return s.FnvHash64(g_strListDiff_fnvHashSeed); }
			Seq Key() const { return m_s; }
		};

		HashMap<PresenceEntry> presenceMapOld;
		presenceMapOld.SetNrBuckets(1 + ((oldEntries.Len()*3)/2));
		for (Seq oldEntry : oldEntries)
			if (!presenceMapOld.Find(oldEntry))
				presenceMapOld.Add(oldEntry);
			
		HashMap<PresenceEntry> presenceMapNew;
		presenceMapNew.SetNrBuckets(1 + ((newEntries.Len()*3)/2));
		for (Seq newEntry : newEntries)
			if (!presenceMapNew.Find(newEntry))
				presenceMapNew.Add(newEntry);

		for (Seq oldEntry : oldEntries)
			if (!presenceMapNew.Find(oldEntry))
				entriesRemoved.Add(oldEntry);

		for (Seq newEntry : newEntries)
			if (!presenceMapOld.Find(newEntry))
				entriesAdded.Add(newEntry);
	}

}
