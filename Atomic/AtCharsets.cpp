#include "AtIncludes.h"
#include "AtCharsets.h"

namespace At
{
	UINT CharsetNameToWindowsCodePage(Seq charset)
	{
		// In practice, many programs that claim ISO-8859-1 actually used Windows 1252. HTML5 recommends that Windows 1252 be used in this case.
		// They are nearly identical, except that Windows 1252 defines useful characters where ISO-8859-1 leaves room for control characters.

		if (charset.EqualInsensitive("utf-8"         )) return 65001;
		if (charset.EqualInsensitive("us-ascii"      )) return 20127;
		if (charset.EqualInsensitive("iso-8859-1"    )) return  1252;
		if (charset.EqualInsensitive("iso-8859-8-i"  )) return 38598;

		if (charset.StripPrefixInsensitive("iso-8859-"))
		{
			UINT n = charset.ReadNrUInt16Dec();
			if (n >= 2 && n <= 16)
				return 28590 + n;
			return 0;
		}

		if (charset.StripPrefixInsensitive("windows-"))
		{
			UINT n = charset.ReadNrUInt16Dec();
			if (n == 874 || (n >= 1250 && n <= 1258))
				return n;
		}

		if (charset.EqualInsensitive("asmo-708"               )) return   708;
		if (charset.EqualInsensitive("big5"                   )) return   950;
		if (charset.EqualInsensitive("cp1025"                 )) return 21025;
		if (charset.EqualInsensitive("cp866"                  )) return   866;
		if (charset.EqualInsensitive("cp875"                  )) return   875;
		if (charset.EqualInsensitive("csiso2022jp"            )) return 50221;
		if (charset.EqualInsensitive("dos-720"                )) return   720;
		if (charset.EqualInsensitive("dos-862"                )) return   862;
		if (charset.EqualInsensitive("euc-cn"                 )) return 51936;
		if (charset.EqualInsensitive("euc-jp"                 )) return 20932;
		if (charset.EqualInsensitive("euc-jp"                 )) return 51932;
		if (charset.EqualInsensitive("euc-kr"                 )) return 51949;
		if (charset.EqualInsensitive("gb18030"                )) return 54936;
		if (charset.EqualInsensitive("gb2312"                 )) return   936;
		if (charset.EqualInsensitive("hz-gb-2312"             )) return 52936;
		if (charset.EqualInsensitive("ibm00858"               )) return   858;
		if (charset.EqualInsensitive("ibm00924"               )) return 20924;
		if (charset.EqualInsensitive("ibm01047"               )) return  1047;
		if (charset.EqualInsensitive("ibm01140"               )) return  1140;
		if (charset.EqualInsensitive("ibm01141"               )) return  1141;
		if (charset.EqualInsensitive("ibm01142"               )) return  1142;
		if (charset.EqualInsensitive("ibm01143"               )) return  1143;
		if (charset.EqualInsensitive("ibm01144"               )) return  1144;
		if (charset.EqualInsensitive("ibm01145"               )) return  1145;
		if (charset.EqualInsensitive("ibm01146"               )) return  1146;
		if (charset.EqualInsensitive("ibm01147"               )) return  1147;
		if (charset.EqualInsensitive("ibm01148"               )) return  1148;
		if (charset.EqualInsensitive("ibm01149"               )) return  1149;
		if (charset.EqualInsensitive("ibm037"                 )) return    37;
		if (charset.EqualInsensitive("ibm1026"                )) return  1026;
		if (charset.EqualInsensitive("ibm273"                 )) return 20273;
		if (charset.EqualInsensitive("ibm277"                 )) return 20277;
		if (charset.EqualInsensitive("ibm278"                 )) return 20278;
		if (charset.EqualInsensitive("ibm280"                 )) return 20280;
		if (charset.EqualInsensitive("ibm284"                 )) return 20284;
		if (charset.EqualInsensitive("ibm285"                 )) return 20285;
		if (charset.EqualInsensitive("ibm290"                 )) return 20290;
		if (charset.EqualInsensitive("ibm297"                 )) return 20297;
		if (charset.EqualInsensitive("ibm420"                 )) return 20420;
		if (charset.EqualInsensitive("ibm423"                 )) return 20423;
		if (charset.EqualInsensitive("ibm424"                 )) return 20424;
		if (charset.EqualInsensitive("ibm437"                 )) return   437;
		if (charset.EqualInsensitive("ibm500"                 )) return   500;
		if (charset.EqualInsensitive("ibm737"                 )) return   737;
		if (charset.EqualInsensitive("ibm775"                 )) return   775;
		if (charset.EqualInsensitive("ibm850"                 )) return   850;
		if (charset.EqualInsensitive("ibm852"                 )) return   852;
		if (charset.EqualInsensitive("ibm855"                 )) return   855;
		if (charset.EqualInsensitive("ibm857"                 )) return   857;
		if (charset.EqualInsensitive("ibm860"                 )) return   860;
		if (charset.EqualInsensitive("ibm861"                 )) return   861;
		if (charset.EqualInsensitive("ibm863"                 )) return   863;
		if (charset.EqualInsensitive("ibm864"                 )) return   864;
		if (charset.EqualInsensitive("ibm865"                 )) return   865;
		if (charset.EqualInsensitive("ibm869"                 )) return   869;
		if (charset.EqualInsensitive("ibm870"                 )) return   870;
		if (charset.EqualInsensitive("ibm871"                 )) return 20871;
		if (charset.EqualInsensitive("ibm880"                 )) return 20880;
		if (charset.EqualInsensitive("ibm905"                 )) return 20905;
		if (charset.EqualInsensitive("ibm-thai"               )) return 20838;
		if (charset.EqualInsensitive("iso-2022-jp"            )) return 50220;
		if (charset.EqualInsensitive("iso-2022-jp"            )) return 50222;
		if (charset.EqualInsensitive("iso-2022-kr"            )) return 50225;
		if (charset.EqualInsensitive("johab"                  )) return  1361;
		if (charset.EqualInsensitive("koi8-r"                 )) return 20866;
		if (charset.EqualInsensitive("koi8-u"                 )) return 21866;
		if (charset.EqualInsensitive("ks_c_5601-1987"         )) return   949;
		if (charset.EqualInsensitive("macintosh"              )) return 10000;
		if (charset.EqualInsensitive("shift_jis"              )) return   932;
		if (charset.EqualInsensitive("utf-7"                  )) return 65000;
		if (charset.EqualInsensitive("x_chinese-eten"         )) return 20002;
		if (charset.EqualInsensitive("x-chinese_cns"          )) return 20000;
		if (charset.EqualInsensitive("x-cp20001"              )) return 20001;
		if (charset.EqualInsensitive("x-cp20003"              )) return 20003;
		if (charset.EqualInsensitive("x-cp20004"              )) return 20004;
		if (charset.EqualInsensitive("x-cp20005"              )) return 20005;
		if (charset.EqualInsensitive("x-cp20261"              )) return 20261;
		if (charset.EqualInsensitive("x-cp20269"              )) return 20269;
		if (charset.EqualInsensitive("x-cp20936"              )) return 20936;
		if (charset.EqualInsensitive("x-cp20949"              )) return 20949;
		if (charset.EqualInsensitive("x-cp50227"              )) return 50227;
		if (charset.EqualInsensitive("x-ebcdic-koreanextended")) return 20833;
		if (charset.EqualInsensitive("x-europa"               )) return 29001;
		if (charset.EqualInsensitive("x-ia5"                  )) return 20105;
		if (charset.EqualInsensitive("x-ia5-german"           )) return 20106;
		if (charset.EqualInsensitive("x-ia5-norwegian"        )) return 20108;
		if (charset.EqualInsensitive("x-ia5-swedish"          )) return 20107;
		if (charset.EqualInsensitive("x-iscii-as"             )) return 57006;
		if (charset.EqualInsensitive("x-iscii-be"             )) return 57003;
		if (charset.EqualInsensitive("x-iscii-de"             )) return 57002;
		if (charset.EqualInsensitive("x-iscii-gu"             )) return 57010;
		if (charset.EqualInsensitive("x-iscii-ka"             )) return 57008;
		if (charset.EqualInsensitive("x-iscii-ma"             )) return 57009;
		if (charset.EqualInsensitive("x-iscii-or"             )) return 57007;
		if (charset.EqualInsensitive("x-iscii-pa"             )) return 57011;
		if (charset.EqualInsensitive("x-iscii-ta"             )) return 57004;
		if (charset.EqualInsensitive("x-iscii-te"             )) return 57005;
		if (charset.EqualInsensitive("x-mac-arabic"           )) return 10004;
		if (charset.EqualInsensitive("x-mac-ce"               )) return 10029;
		if (charset.EqualInsensitive("x-mac-chinesesimp"      )) return 10008;
		if (charset.EqualInsensitive("x-mac-chinesetrad"      )) return 10002;
		if (charset.EqualInsensitive("x-mac-croatian"         )) return 10082;
		if (charset.EqualInsensitive("x-mac-cyrillic"         )) return 10007;
		if (charset.EqualInsensitive("x-mac-greek"            )) return 10006;
		if (charset.EqualInsensitive("x-mac-hebrew"           )) return 10005;
		if (charset.EqualInsensitive("x-mac-icelandic"        )) return 10079;
		if (charset.EqualInsensitive("x-mac-japanese"         )) return 10001;
		if (charset.EqualInsensitive("x-mac-korean"           )) return 10003;
		if (charset.EqualInsensitive("x-mac-romanian"         )) return 10010;
		if (charset.EqualInsensitive("x-mac-thai"             )) return 10021;
		if (charset.EqualInsensitive("x-mac-turkish"          )) return 10081;
		if (charset.EqualInsensitive("x-mac-ukrainian"        )) return 10017;
		
		return 0;
	}
}
