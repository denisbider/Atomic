#include "AutIncludes.h"


void WinErrTest(int argc, char** argv)
{
	if (argc < 3)
		throw "Missing error code";

	Seq   errStr { argv[2] };
	int64 err    {};

	if (errStr.StripPrefixInsensitive("0x"))
		err = errStr.ReadNrSInt64(16);
	else
		err = errStr.ReadNrSInt64Dec();

	Console::Out(Str("As Windows error:\r\n").Fun(DescribeWinErr, err).Add("\r\n\r\n"));
	Console::Out(Str("As NTSTATUS:\r\n").Fun(DescribeNtStatus, err).Add("\r\n"));
}
