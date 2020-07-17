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
	TEST_STR_EQ(Str("a").Chars(100, 'a').Chars(1000, 'a').ResizeExact(5), "aaaaa");
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

	Time t1 = Time::FromIsoStyleTimeStr("2019-03-06 22:16:23.012034").Ref();
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

	Time t2 = Time::FromIsoStyleTimeStr("2020-02-29 22:16:23.012034").Ref();
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

	Seq ts;
	ts = "0";                           EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "0"            ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2";                           EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "2"            ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "202";                         EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "202"          ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2020";                        EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "2020"         ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2020-";                       EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "2020-"        ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2020-07";                     EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "2020-07"      ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2020-07-";                    EnsureThrow(!ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "2020-07-"     ); TEST_STR_EQ(Str().UInt(t1.ToFt()), "0");
	ts = "2020-07-16";                  EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16x";                 EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16 x";                EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16tx";                EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16Tx";                EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16 0";                EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "0"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16 24";               EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "24"           ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16 00";               EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 00:00:00.000000Z");
	ts = "2020-07-16 23x";              EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 23:00:00.000000Z");
	ts = "2020-07-16 23:";              EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 23:00:00.000000Z");
	ts = "2020-07-16 10:0";             EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "0"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:00:00.000000Z");
	ts = "2020-07-16 10:60";            EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "60"           ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:00:00.000000Z");
	ts = "2020-07-16 10:00";            EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:00:00.000000Z");
	ts = "2020-07-16 10:34";            EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:00.000000Z");
	ts = "2020-07-16 10:34x";           EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:00.000000Z");
	ts = "2020-07-16 10:34:";           EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:00.000000Z");
	ts = "2020-07-16 10:34:60";         EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "60"           ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:00.000000Z");
	ts = "2020-07-16 10:34:59";         EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.000000Z");
	ts = "2020-07-16 10:34:59x";        EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.000000Z");
	ts = "2020-07-16 10:34:59.";        EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.000000Z");
	ts = "2020-07-16 10:34:59.x";       EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "x"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.000000Z");
	ts = "2020-07-16 10:34:59.012";     EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.012000Z");
	ts = "2020-07-16 10:34:59.012034";  EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, ""             ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.012034Z");
	ts = "2020-07-16 10:34:59.012034Z"; EnsureThrow( ts.ReadIsoStyleTimeStr(t1)); TEST_STR_EQ(ts, "Z"            ); TEST_STR_EQ(Str::From(t1, TimeFmt::IsoMicroZ), "2020-07-16 10:34:59.012034Z");

	TEST_STR_EQ(Str::From(Time(), TimeFmt::IsoMicroZ), "1601-01-01 00:00:00.000000Z");
	TEST_STR_EQ(Str().UInt(Time::FromIsoStyleTimeStr("1601-01-01T00:00:00.000000Z")->ToFt()), "0");
	TEST_STR_EQ(Str().UInt(Time::FromIsoStyleTimeStr("1601-01-01T00:00:00.000001Z")->ToFt()), "10");

	TEST_STR_EQ(Str().UIntDecGrp(0),        "0");
	TEST_STR_EQ(Str().UIntDecGrp(1),        "1");
	TEST_STR_EQ(Str().UIntDecGrp(999),      "999");
	TEST_STR_EQ(Str().UIntDecGrp(1000),     "1,000");
	TEST_STR_EQ(Str().UIntDecGrp(1001),     "1,001");
	TEST_STR_EQ(Str().UIntDecGrp(1999),     "1,999");
	TEST_STR_EQ(Str().UIntDecGrp(2000),     "2,000");
	TEST_STR_EQ(Str().UIntDecGrp(2001),     "2,001");
	TEST_STR_EQ(Str().UIntDecGrp(2523),     "2,523");
	TEST_STR_EQ(Str().UIntDecGrp(9999),     "9,999");
	TEST_STR_EQ(Str().UIntDecGrp(10000),    "10,000");
	TEST_STR_EQ(Str().UIntDecGrp(10999),    "10,999");
	TEST_STR_EQ(Str().UIntDecGrp(11000),    "11,000");
	TEST_STR_EQ(Str().UIntDecGrp(11001),    "11,001");
	TEST_STR_EQ(Str().UIntDecGrp(100999),   "100,999");
	TEST_STR_EQ(Str().UIntDecGrp(101000),   "101,000");
	TEST_STR_EQ(Str().UIntDecGrp(101001),   "101,001");
	TEST_STR_EQ(Str().UIntDecGrp(999999),   "999,999");
	TEST_STR_EQ(Str().UIntDecGrp(1000000),  "1,000,000");
	TEST_STR_EQ(Str().UIntDecGrp(1000001),  "1,000,001");
	TEST_STR_EQ(Str().UIntDecGrp(1000999),  "1,000,999");
	TEST_STR_EQ(Str().UIntDecGrp(1001000),  "1,001,000");
	TEST_STR_EQ(Str().UIntDecGrp(1999999),  "1,999,999");
	TEST_STR_EQ(Str().UIntDecGrp(2000000),  "2,000,000");
	TEST_STR_EQ(Str().UIntDecGrp(2000001),  "2,000,001");
	TEST_STR_EQ(Str().UIntDecGrp(9999999),  "9,999,999");
	TEST_STR_EQ(Str().UIntDecGrp(10000000), "10,000,000");
	TEST_STR_EQ(Str().UIntDecGrp(10000001), "10,000,001");
	TEST_STR_EQ(Str().UIntDecGrp(10000999), "10,000,999");
	TEST_STR_EQ(Str().UIntDecGrp(10001000), "10,001,000");
	TEST_STR_EQ(Str().UIntDecGrp(999999999999999999ULL),   "999,999,999,999,999,999");
	TEST_STR_EQ(Str().UIntDecGrp(1000000000000000000ULL),  "1,000,000,000,000,000,000");
	TEST_STR_EQ(Str().UIntDecGrp(1000000000000000001ULL),  "1,000,000,000,000,000,001");
	TEST_STR_EQ(Str().UIntDecGrp(18446744073709550999ULL), "18,446,744,073,709,550,999");
	TEST_STR_EQ(Str().UIntDecGrp(18446744073709551000ULL), "18,446,744,073,709,551,000");
	TEST_STR_EQ(Str().UIntDecGrp(18446744073709551614ULL), "18,446,744,073,709,551,614");
	TEST_STR_EQ(Str().UIntDecGrp(18446744073709551615ULL), "18,446,744,073,709,551,615");

	TEST_STR_EQ(Str().UIntBytes(0),       "0 B");
	TEST_STR_EQ(Str().UIntBytes(1),       "1 B");
	TEST_STR_EQ(Str().UIntBytes(9),       "9 B");
	TEST_STR_EQ(Str().UIntBytes(10),      "10 B");
	TEST_STR_EQ(Str().UIntBytes(11),      "11 B");
	TEST_STR_EQ(Str().UIntBytes(99),      "99 B");
	TEST_STR_EQ(Str().UIntBytes(100),     "100 B");
	TEST_STR_EQ(Str().UIntBytes(101),     "101 B");
	TEST_STR_EQ(Str().UIntBytes(999),     "999 B");
	TEST_STR_EQ(Str().UIntBytes(1000),    "1,000 B");
	TEST_STR_EQ(Str().UIntBytes(1001),    "1,001 B");
	TEST_STR_EQ(Str().UIntBytes(1023),    "1,023 B");
	TEST_STR_EQ(Str().UIntBytes(1024),    "1.00 kB");
	TEST_STR_EQ(Str().UIntBytes(1025),    "1.00 kB");
	TEST_STR_EQ(Str().UIntBytes(1029),    "1.00 kB");
	TEST_STR_EQ(Str().UIntBytes(1030),    "1.01 kB");
	TEST_STR_EQ(Str().UIntBytes(1121),    "1.09 kB");
	TEST_STR_EQ(Str().UIntBytes(1122),    "1.10 kB");
	TEST_STR_EQ(Str().UIntBytes(1123),    "1.10 kB");
	TEST_STR_EQ(Str().UIntBytes(1131),    "1.10 kB");
	TEST_STR_EQ(Str().UIntBytes(1132),    "1.11 kB");
	TEST_STR_EQ(Str().UIntBytes(10187),   "9.95 kB");
	TEST_STR_EQ(Str().UIntBytes(10234),   "9.99 kB");
	TEST_STR_EQ(Str().UIntBytes(10235),   "10.0 kB");
	TEST_STR_EQ(Str().UIntBytes(10236),   "10.0 kB");
	TEST_STR_EQ(Str().UIntBytes(10240),   "10.0 kB");
	TEST_STR_EQ(Str().UIntBytes(10291),   "10.0 kB");
	TEST_STR_EQ(Str().UIntBytes(10292),   "10.1 kB");
	TEST_STR_EQ(Str().UIntBytes(101888),  "99.5 kB");
	TEST_STR_EQ(Str().UIntBytes(102348),  "99.9 kB");
	TEST_STR_EQ(Str().UIntBytes(102349),  "100 kB");
	TEST_STR_EQ(Str().UIntBytes(102911),  "100 kB");
	TEST_STR_EQ(Str().UIntBytes(102912),  "101 kB");
	TEST_STR_EQ(Str().UIntBytes(1023487), "999 kB");
	TEST_STR_EQ(Str().UIntBytes(1023488), "1,000 kB");
	TEST_STR_EQ(Str().UIntBytes(1024511), "1,000 kB");
	TEST_STR_EQ(Str().UIntBytes(1024512), "1,001 kB");
	TEST_STR_EQ(Str().UIntBytes(1047552), "1,023 kB");
	TEST_STR_EQ(Str().UIntBytes(1048063), "1,023 kB");
	TEST_STR_EQ(Str().UIntBytes(1048064), "1.00 MB");
	TEST_STR_EQ(Str().UIntBytes(1048575), "1.00 MB");
	TEST_STR_EQ(Str().UIntBytes(1048576), "1.00 MB");
	TEST_STR_EQ(Str().UIntBytes(1053818), "1.00 MB");
	TEST_STR_EQ(Str().UIntBytes(1053819), "1.01 MB");
	TEST_STR_EQ(Str().UIntBytes(1125350151028735), "1,023 TB");
	TEST_STR_EQ(Str().UIntBytes(1125350151028736), "1,024 TB");
	TEST_STR_EQ(Str().UIntBytes(1125899906842624), "1,024 TB");
	TEST_STR_EQ(Str().UIntBytes(18446743523953737727ULL), "16,777,215 TB");
	TEST_STR_EQ(Str().UIntBytes(18446743523953737728ULL), "16,777,216 TB");
	TEST_STR_EQ(Str().UIntBytes(18446744073709551615ULL), "16,777,216 TB");
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
	TEST_EXCEPTION_TYPE(Str().ResizeExact(SIZE_MAX), At::InternalInconsistency);
	TEST_EXCEPTION_TYPE(Str().ResizeExact(SIZE_MAX-1), std::bad_alloc);
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
	catch (std::exception const& e) { Console::Out(Str("\r\nEnsureThrow test - verify output visually:\r\n-begin-\r\n").Add(e.what()).Add("-end-\r\n")); }
			
	try { EnsureThrowWithNr(!"EnsureThrowWithNr test", 42); }
	catch (std::exception const& e) { Console::Out(Str("\r\nEnsureThrowWithNr test - verify output visually:\r\n-begin-\r\n").Add(e.what()).Add("-end-\r\n")); }
			
	try { EnsureThrowWithNr2(!"EnsureThrowWithNr2 test", 100, 103); }
	catch (std::exception const& e) { Console::Out(Str("\r\nEnsureThrowWithNr2 test - verify output visually:\r\n-begin-\r\n").Add(e.what()).Add("-end-\r\n")); }
			
	try { EnsureFailWithDesc(OnFail::Throw, "dummy error description", __FUNCTION__, __LINE__); }
	catch (std::exception const& e) { Console::Out(Str("\r\nEnsureFailWithDesc test - verify output visually:\r\n-begin-\r\n").Add(e.what()).Add("-end-\r\n")); }

	Console::Out("\r\nEnsureReportHandler_Interactive test - verify output visually:\r\n-begin-\r\n");
	EnsureReportHandler_Interactive("Sample Ensure failure output goes here.\r\n"
		"Aa Oo Uu: \xC3\x84\xC3\xA4 \xC3\x96\xC3\xB6 \xC3\x9C\xC3\xBC\r\n"
		"Cc Ss Zz: \xC4\x8C\xC4\x8D \xC5\xA0\xC5\xA1 \xC5\xBD\xC5\xBE\r\n");
	Console::Out("-end-\r\n");

	_set_se_translator(SeTranslator);

	try { ((byte*) nullptr)[0] = 0; }
	catch (std::exception const& e) { Console::Out(Str("\r\nNull pointer write test - verify output visually:\r\n-begin-\r\n").Add(e.what()).Add("-end-\r\n")); }

	{
		bool ok = (EnsureFailDesc::s_nrAllocated == EnsureFailDesc::s_nrFreed);
		char const* okOrFail = (ok ? " - ok\r\n" : " - FAIL\r\n");
		Console::Out(Str("\r\nEnsureFailDesc::s_nrAllocated: ").SInt(EnsureFailDesc::s_nrAllocated).Add(", s_nrFreed: ").SInt(EnsureFailDesc::s_nrFreed).Add(okOrFail));
		EnsureThrow(ok);
	}
}


void CoreTests()
{
	CoreTests_Str();
	CoreTests_Heap();
	CoreTests_HashMap();
	CoreTests_Exceptions();
}
