#include "AutIncludes.h"
#include "AutMain.h"


void TestDesc(Seq desc)
{
	Console::Out(desc);
	if (desc.n < 95)
		Console::Out(Str().Chars(95 - desc.n, ' '));
}


void TestStr(Seq desc, Str const& left, Str const& right, bool expected)
{
	TestDesc(desc);

	bool result = (left == right);
	if (result == expected)
		Console::Out("ok\r\n");
	else
	{
		Str msg = "FAIL\r\n";
		msg.Add("  l: ").Add(left).Add(" (hex: ").Hex(left).Add(")\r\n")
		   .Add("  r: ").Add(right).Add(" (hex: ").Hex(right).Add(")\r\n");
		Console::Out(msg);
	}
};

#define TEST_STR_EQ(LEFT, RIGHT) TestStr(#LEFT " == " #RIGHT, (LEFT), (RIGHT), true)
#define TEST_STR_NE(LEFT, RIGHT) TestStr(#LEFT " != " #RIGHT, (LEFT), (RIGHT), false)


void CoreTests_Str()
{
	Console::Out(Str("sizeof(Str): ").UInt(sizeof(Str)).Add("\r\n"));

	TEST_STR_EQ(Str(), "");
	TEST_STR_EQ(Str("a"), "a");
	TEST_STR_EQ(Str().Add("a"), "a");
	TEST_STR_EQ(Str().Ch('a'), "a");
	TEST_STR_EQ(Str().Ch('a').Ch('b'), "ab");
	TEST_STR_EQ(Str().Chars(1, 'a'), "a");
	TEST_STR_EQ(Str().Chars(1, 'a').Ch('b'), "ab");
	TEST_STR_EQ(Str().Ch('a').Chars(1, 'b'), "ab");
	TEST_STR_EQ(Str().Chars(2, 'a'), "aa");

	TEST_STR_NE(Str(), "a");
	TEST_STR_NE(Str("a"), "");
	TEST_STR_NE(Str().Add("a"), "b");
	TEST_STR_NE(Str().Ch('a'), "");
	TEST_STR_NE(Str().Chars(2, 'a'), "a");

	TEST_STR_EQ(Str("a").Chars(100, 'a'), Str().Chars(101, 'a'));
	TEST_STR_EQ(Str("a").Chars(100, 'a').Chars(1000, 'a'), Str().Chars(1101, 'a'));
	TEST_STR_EQ(Str("a").Chars(100, 'a').Chars(1000, 'a').Resize(5), "aaaaa");
	TEST_STR_EQ(Seq(Str("a").Chars(100, 'a').Chars(1000, 'a').Byte(0).CharPtr()), Str().Chars(1101, 'a'));

	TEST_STR_EQ(ToUtf8Norm("\xE1\xE9\xED\xF3\xFA", 1250), "\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA");

	TEST_STR_EQ(Seq(Str().Dbl(0.0, 0)), "0.");
	TEST_STR_EQ(Seq(Str().Dbl(0.0, 1)), "0.0");
	TEST_STR_EQ(Seq(Str().Dbl(0.0, 2)), "0.00");
	TEST_STR_EQ(Seq(Str().Dbl(0.1, 0)), "0.");
	TEST_STR_EQ(Seq(Str().Dbl(0.1, 1)), "0.1");
	TEST_STR_EQ(Seq(Str().Dbl(0.1, 2)), "0.10");
	TEST_STR_EQ(Seq(Str().Dbl(0.51, 0)), "1.");
	TEST_STR_EQ(Seq(Str().Dbl(0.51, 1)), "0.5");
	TEST_STR_EQ(Seq(Str().Dbl(0.51, 2)), "0.51");
	TEST_STR_EQ(Seq(Str().Dbl(0.51, 3)), "0.510");
	TEST_STR_EQ(Seq(Str().Dbl(123.06, 0)), "123.");
	TEST_STR_EQ(Seq(Str().Dbl(123.06, 1)), "123.1");
	TEST_STR_EQ(Seq(Str().Dbl(123.06, 2)), "123.06");
	TEST_STR_EQ(Seq(Str().Dbl(123.06, 3)), "123.060");
	TEST_STR_EQ(Seq(Str().Dbl(123.56, 0)), "124.");
	TEST_STR_EQ(Seq(Str().Dbl(123.56, 1)), "123.6");
	TEST_STR_EQ(Seq(Str().Dbl(123.56, 2)), "123.56");
	TEST_STR_EQ(Seq(Str().Dbl(123.56, 3)), "123.560");
	TEST_STR_EQ(Seq(Str().Dbl(-123.56, 3)), "-123.560");

	Time t1 = Time::FromIsoStyleTimeStr("2019-03-06 22:16:23.012034");
	TEST_STR_EQ(Str::From(t1, TimeFmt::EngDateAbbr ), "Mar 6, 2019");
	TEST_STR_EQ(Str::From(t1, TimeFmt::EngDateFull ), "March 6, 2019");
	TEST_STR_EQ(Str::From(t1, TimeFmt::YyyyMmDd    ), "2019-03-06");
	TEST_STR_EQ(Str::From(t1, TimeFmt::Http        ), "Wed, 06 Mar 2019 22:16:23 GMT");
	TEST_STR_EQ(Str::From(t1, TimeFmt::Email       ), "Wed, 6 Mar 2019 22:16:23 +0000");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoSec      ), "2019-03-06 22:16:23");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoSecZ     ), "2019-03-06 22:16:23Z");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMilli    ), "2019-03-06 22:16:23.012");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMilliZ   ), "2019-03-06 22:16:23.012Z");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicro    ), "2019-03-06 22:16:23.012034");
	TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ   ), "2019-03-06 22:16:23.012034Z");
	TEST_STR_EQ(Str::From(t1, TimeFmt::Dense       ), "20190306-221623");

	Time t2 = Time::FromIsoStyleTimeStr("2020-02-29 22:16:23.012034");
	TEST_STR_EQ(Str::From(t2, TimeFmt::EngDateAbbr ), "Feb 29, 2020");
	TEST_STR_EQ(Str::From(t2, TimeFmt::EngDateFull ), "February 29, 2020");
	TEST_STR_EQ(Str::From(t2, TimeFmt::YyyyMmDd    ), "2020-02-29");
	TEST_STR_EQ(Str::From(t2, TimeFmt::Http        ), "Sat, 29 Feb 2020 22:16:23 GMT");
	TEST_STR_EQ(Str::From(t2, TimeFmt::Email       ), "Sat, 29 Feb 2020 22:16:23 +0000");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoSec      ), "2020-02-29 22:16:23");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoSecZ     ), "2020-02-29 22:16:23Z");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoMilli    ), "2020-02-29 22:16:23.012");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoMilliZ   ), "2020-02-29 22:16:23.012Z");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoMicro    ), "2020-02-29 22:16:23.012034");
	TEST_STR_EQ(Str::From(t2, TimeFmt::IsoMicroZ   ), "2020-02-29 22:16:23.012034Z");
	TEST_STR_EQ(Str::From(t2, TimeFmt::Dense       ), "20200229-221623");

	Time t3 = Time::MaxDisplayable();
	TEST_STR_EQ(Str::From(t3, TimeFmt::EngDateAbbr ), "Sep 14, 30828");
	TEST_STR_EQ(Str::From(t3, TimeFmt::EngDateFull ), "September 14, 30828");
	TEST_STR_EQ(Str::From(t3, TimeFmt::YyyyMmDd    ), "30828-09-14");
	TEST_STR_EQ(Str::From(t3, TimeFmt::Http        ), "Thu, 14 Sep 30828 02:48:05 GMT");
	TEST_STR_EQ(Str::From(t3, TimeFmt::Email       ), "Thu, 14 Sep 30828 02:48:05 +0000");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoSec      ), "30828-09-14 02:48:05");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoSecZ     ), "30828-09-14 02:48:05Z");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoMilli    ), "30828-09-14 02:48:05.477");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoMilliZ   ), "30828-09-14 02:48:05.477Z");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoMicro    ), "30828-09-14 02:48:05.477580");
	TEST_STR_EQ(Str::From(t3, TimeFmt::IsoMicroZ   ), "30828-09-14 02:48:05.477580Z");
	TEST_STR_EQ(Str::From(t3, TimeFmt::Dense       ), "308280914-024805");
}


void CoreTests_Heap()
{
	struct Tracker
	{
		int& m_counter;
		int m_value {};

		Tracker(int& counter) : m_counter(counter) { ++m_counter; }
		~Tracker() noexcept { --m_counter; }
	};

	using H = Heap<Tracker>;
	using E = H::Entry;

	H h;

	enum { N = 8 };
	E* entries[N] = {};
	int nrEntries {};
	int counter {};

	auto addEntry = [&] ()
		{
			EnsureThrow(nrEntries < N);
			EnsureThrow(entries[nrEntries] == nullptr);
			E& e = h.Add();
			EnsureThrow(!e.m_next);
			EnsureThrow(!e.m_x.Any());
			e.m_next = (E*) (sizet) nrEntries;
			e.m_x.Init(counter).m_value = nrEntries;
			entries[nrEntries++] = &e;
		};

	auto eraseEntry = [&] (int i)
		{
			EnsureThrow(i < N);
			EnsureThrow(entries[i] != nullptr);
			E& e = *entries[i];
			EnsureThrow(e.m_next == (E*) (sizet) i);
			EnsureThrow(e.m_x.Ref().m_value == i);
			h.Erase(e);
			entries[i] = nullptr;
		};

	auto verify = [&] (int presentMap[N])
		{
			Str msg = "Verified: ";
			int expectedCounter {};
			for (int i=0; i!=N; ++i)
				if (!presentMap[i])
				{
					EnsureThrow(entries[i] == nullptr);
					msg.Ch('0');
				}
				else
				{
					++expectedCounter;
					EnsureThrow(entries[i] != nullptr);
					E& e = *entries[i];
					EnsureThrow(e.m_next == (E*) (sizet) i);
					EnsureThrow(e.m_x.Ref().m_value == i);
					msg.Ch('1');
				}

			EnsureThrow(expectedCounter == counter);
			msg.Add(", counter ").SInt(counter).Add("\r\n");
			Console::Out(msg);
		};

	addEntry();                         {int p[N]={1};               verify(p);} eraseEntry(0);                               {int p[N]={};                verify(p);}
	addEntry(); addEntry();             {int p[N]={0,1,1};           verify(p);} eraseEntry(1); eraseEntry(2);                {int p[N]={};                verify(p);}
	addEntry(); addEntry(); addEntry(); {int p[N]={0,0,0,1,1,1};     verify(p);} eraseEntry(4);                               {int p[N]={0,0,0,1,0,1};     verify(p);}
	addEntry(); addEntry();             {int p[N]={0,0,0,1,0,1,1,1}; verify(p);} eraseEntry(3); eraseEntry(6); eraseEntry(7); {int p[N]={0,0,0,0,0,1,0,0}; verify(p);}
		                                                                         eraseEntry(5);                               {int p[N]={0,0,0,0,0,0,0,0}; verify(p);}
}


void CoreTests_HashMap()
{
	struct KeyVal
	{
		sizet m_k {};
		sizet m_v {};

		KeyVal(sizet k, sizet v) : m_k(k), m_v(v) {}

		sizet Key() const { return m_k; }
		static sizet HashOfKey(sizet k) { return k; }
	};

	HashMap<KeyVal> m;
	m.SetNrBuckets(7);

	sizet nrAdded {}, nrErased {};
	OrderedSet<sizet> erasedSet;
	for (sizet j=0; j!=10; ++j)
		for (sizet i=0; i!=1000; ++i)
			if ((i%3) != (j%3))
				m.Add(i, nrAdded++);
			else
			{
				KeyVal* kv = m.Find(i);
				if (!kv)
					EnsureThrow(!m.Erase(i));
				else
				{
					EnsureThrow(kv->m_k == i);
					EnsureThrow(!erasedSet.Contains(kv->m_v));
					erasedSet.Add(kv->m_v);
					EnsureThrow(m.Erase(i));
					++nrErased;
				}
			}

	Console::Out(Str("HashMap: ").UInt(nrAdded).Add(" added, ").UInt(nrErased).Add(" erased, erasedSet: [")
		.UInt(erasedSet.First()).Add(", ..., ").UInt(erasedSet.Last()).Add("]\r\n"));

	for (sizet i=0; i!=1000; ++i)
		while (true)
		{
			KeyVal* kv = m.Find(i);
			if (!kv)
				break;

			EnsureThrow(kv->m_k == i);
			EnsureThrow(!erasedSet.Contains(kv->m_v));
			erasedSet.Add(kv->m_v);
			EnsureThrow(m.Erase(i));
			++nrErased;
		}
		
	EnsureThrow(nrErased == nrAdded);
	EnsureThrow(erasedSet.Len() == nrAdded);
	EnsureThrow(erasedSet.First() == 0);
	EnsureThrow(erasedSet.Last() == nrAdded-1);
	Console::Out(Str("HashMap: ").UInt(nrAdded).Add(" added, ").UInt(nrErased).Add(" erased, erasedSet: [")
		.UInt(erasedSet.First()).Add(", ..., ").UInt(erasedSet.Last()).Add("]\r\n"));
}


template <typename ExpectedExceptionType>
void TestExceptionType(Seq desc, std::function<void()> test)
{
	TestDesc(desc);

	try
	{
		test();
	}
	catch (ExpectedExceptionType const&)
	{
		Console::Out("ok\r\n");
	}
	catch (...)
	{
		Console::Out("FAIL\r\n");
		throw;
	}
}

#define TEST_EXCEPTION_TYPE(TEST, TYPE) TestExceptionType<TYPE>(#TEST ": " #TYPE, []() { TEST; })


void CoreTests_Exceptions()
{
	TEST_EXCEPTION_TYPE(Str().Resize(SIZE_MAX), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Resize(SIZE_MAX-1), std::bad_alloc);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ReserveInc(SIZE_MAX), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ReserveInc(SIZE_MAX-1000), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ReserveInc(SIZE_MAX-1001), std::bad_alloc);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ResizeInc(SIZE_MAX), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ResizeInc(SIZE_MAX-1000), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').ResizeInc(SIZE_MAX-1001), std::bad_alloc);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').Chars(SIZE_MAX, 'b'), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').Chars(SIZE_MAX-1000, 'b'), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().Chars(1000, 'a').Chars(SIZE_MAX-1001, 'b'), std::bad_alloc);

	try { EnsureThrow(!"EnsureThrow test"); }
	catch (std::exception const& e) { Console::Out(Str("EnsureThrow test - verify manually:\r\n").Add(e.what())); }
			
	try { EnsureThrowWithNr(!"EnsureThrowWithNr test", 42); }
	catch (std::exception const& e) { Console::Out(Str("EnsureThrowWithNr test - verify manually:\r\n").Add(e.what())); }
}


void CoreTests()
{
	CoreTests_Str();
	CoreTests_Heap();
	CoreTests_HashMap();
	CoreTests_Exceptions();
}
