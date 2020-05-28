#include "AutIncludes.h"
#include "AutMain.h"


struct DataWithStrKey : RefCountable
{
	Str m_k;
	int m_i {};

	Str const& Key() const { return m_k; }
};


struct DataWithIntKey : RefCountable
{
	int m_k {};
	Str m_s;

	int Key() const { return m_k; }
};


void MapTests()
{
	DataWithIntKey dwik;
	DataWithStrKey dwsk;
	Rp<DataWithIntKey> spDwik { new DataWithIntKey };
	Rp<DataWithStrKey> spDwsk { new DataWithStrKey };

	{ Map<    DataWithIntKey>  m; m.Add(dwik);   for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ Map<    DataWithStrKey>  m; m.Add(dwsk);   for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ Map< Rp<DataWithIntKey>> m; m.Add(spDwik); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ Map< Rp<DataWithStrKey>> m; m.Add(spDwsk); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ MapR<   DataWithIntKey>  m; m.Add(dwik);   for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ MapR<   DataWithStrKey>  m; m.Add(dwsk);   for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ MapR<Rp<DataWithIntKey>> m; m.Add(spDwik); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ MapR<Rp<DataWithStrKey>> m; m.Add(spDwsk); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ OrderedSet<int>  m; int x; m.Add(x); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ OrderedSet<Str>  m; Str x; m.Add(x); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ OrderedSetR<int> m; int x; m.Add(x); for (auto val : m) { m.Find(m.GetKey(val)); } }
	{ OrderedSetR<Str> m; Str x; m.Add(x); for (auto val : m) { m.Find(m.GetKey(val)); } }

	enum { CrcMask = 0xFFFFF, AddEnd = (CrcMask*3)/2, EraseEnd = 3*CrcMask };
	auto crcMasked = [] (uint x) -> uint { return Crc32(Seq(&x, sizeof(uint))) & CrcMask; };
	OrderedSet<uint> os;
	sizet nrAdded {}, nrDup {};
	uint addedLo = UINT_MAX, addedHi = UINT_MAX;

	for (uint i=0; i!=AddEnd; ++i)
	{
		uint v = crcMasked(i);
		bool added {};
		OrderedSet<uint>::It it = os.FindOrAdd(added, v);
		if (added) ++nrAdded; else ++nrDup;
		EnsureThrow(*it == v);
		OrderedSet<uint>::It itPrev = it; --itPrev;
		OrderedSet<uint>::It itNext = it; ++itNext;

		if (addedLo > v)
		{
			addedLo = v;
			EnsureThrow(!itPrev.Any());
		}
		else if (addedLo == v)
		{
			EnsureThrow(!added);
			EnsureThrow(!itPrev.Any());
		}
		else
		{
			EnsureThrow(itPrev.Any());
			EnsureThrow(*itPrev < v);
		}

		if (addedHi == UINT_MAX || addedHi < v)
		{
			addedHi = v;
			EnsureThrow(!itNext.Any());
		}
		else if (addedHi == v)
		{
			EnsureThrow(!added);
			EnsureThrow(!itNext.Any());
		}
		else
		{
			EnsureThrow(itNext.Any());
			EnsureThrow(*itNext > v);
		}
	}

	Console::Out(Str("Map: ").UInt(nrAdded).Add(" added, ").UInt(nrDup).Add(" duplicate\r\n"));

	sizet nrErased {}, nrNotFound {};

	for (uint i=AddEnd; i!=EraseEnd; ++i)
	{
		uint v = crcMasked(i);
		OrderedSet<uint>::It it = os.Find(v);
		if (!it.Any())
			++nrNotFound;
		else
		{
			EnsureThrow(*it == v);

			uint prevVal = UINT_MAX, nextVal = UINT_MAX;
			{ OrderedSet<uint>::It itPrev = it; --itPrev; if (itPrev.Any()) { prevVal = *itPrev; EnsureThrowWithNr2(prevVal < v, prevVal, v); } }
			{ OrderedSet<uint>::It itNext = it; ++itNext; if (itNext.Any()) { nextVal = *itNext; EnsureThrowWithNr2(nextVal > v, nextVal, v); } }

			it = os.Erase(it);
			++nrErased;

			if (UINT_MAX != prevVal)
			{
				OrderedSet<uint>::It itPrev = it; --itPrev;
				EnsureThrow(itPrev.Any());
				EnsureThrow(*itPrev == prevVal);
			}

			if (UINT_MAX != nextVal)
			{
				EnsureThrow(it.Any());
				EnsureThrow(*it == nextVal);
			}
		}
	}

	Console::Out(Str("Map: ").UInt(nrErased).Add(" erased, ").UInt(nrNotFound).Add(" not found, ").UInt(os.Len()).Add(" remaining\r\n"));
	Console::Out("Map tests OK\r\n");
}
