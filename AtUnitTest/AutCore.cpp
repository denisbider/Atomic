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


void CoreTests()
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
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::EngDateAbbr ), "Mar 6, 2019");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::EngDateFull ), "March 6, 2019");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::YyyyMmDd    ), "2019-03-06");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::Http        ), "Wed, 06 Mar 2019 22:16:23 GMT");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::Email       ), "Wed, 6 Mar 2019 22:16:23 +0000");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoSec      ), "2019-03-06 22:16:23");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoSecZ     ), "2019-03-06 22:16:23Z");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoMilli    ), "2019-03-06 22:16:23.012");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoMilliZ   ), "2019-03-06 22:16:23.012Z");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoMicro    ), "2019-03-06 22:16:23.012034");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::IsoMicroZ   ), "2019-03-06 22:16:23.012034Z");
	TEST_STR_EQ(Str().Obj(t1, TimeFmt::Dense       ), "20190306-221623");

	Time t2 = Time::FromIsoStyleTimeStr("2020-02-29 22:16:23.012034");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::EngDateAbbr ), "Feb 29, 2020");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::EngDateFull ), "February 29, 2020");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::YyyyMmDd    ), "2020-02-29");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::Http        ), "Sat, 29 Feb 2020 22:16:23 GMT");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::Email       ), "Sat, 29 Feb 2020 22:16:23 +0000");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoSec      ), "2020-02-29 22:16:23");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoSecZ     ), "2020-02-29 22:16:23Z");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoMilli    ), "2020-02-29 22:16:23.012");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoMilliZ   ), "2020-02-29 22:16:23.012Z");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoMicro    ), "2020-02-29 22:16:23.012034");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::IsoMicroZ   ), "2020-02-29 22:16:23.012034Z");
	TEST_STR_EQ(Str().Obj(t2, TimeFmt::Dense       ), "20200229-221623");

	Time t3 = Time::MaxDisplayable();
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::EngDateAbbr ), "Sep 14, 30828");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::EngDateFull ), "September 14, 30828");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::YyyyMmDd    ), "30828-09-14");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::Http        ), "Thu, 14 Sep 30828 02:48:05 GMT");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::Email       ), "Thu, 14 Sep 30828 02:48:05 +0000");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoSec      ), "30828-09-14 02:48:05");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoSecZ     ), "30828-09-14 02:48:05Z");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoMilli    ), "30828-09-14 02:48:05.477");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoMilliZ   ), "30828-09-14 02:48:05.477Z");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoMicro    ), "30828-09-14 02:48:05.477580");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::IsoMicroZ   ), "30828-09-14 02:48:05.477580Z");
	TEST_STR_EQ(Str().Obj(t3, TimeFmt::Dense       ), "308280914-024805");

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
			
	try { EnsureThrowWithCode(!"EnsureThrowWithCode test", 42); }
	catch (std::exception const& e) { Console::Out(Str("EnsureThrowWithCode test - verify manually:\r\n").Add(e.what())); }

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
}
