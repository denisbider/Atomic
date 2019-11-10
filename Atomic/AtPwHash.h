#pragma once

#include "AtBusyBeaver.h"
#include "AtStr.h"

namespace At
{
	struct PwHash
	{
		enum { BB_NrOps = 100000,
			   BB_HdrLen = 1 + 4 + 1,
			   BB_NonceLen = 10,
			   BB_SaltLen = BB_HdrLen + BB_NonceLen,
			   Len = BB_SaltLen + BusyBeaver<BB_SHA512>::OutputSize };

		static void Generate(Seq pw, byte* result);
		static Str Generate(Seq pw) { Str result; result.Resize(Len); Generate(pw, result.Ptr()); return result; }

		static bool Verify(Seq pw, Seq pwHash);

		// A function with similar side channel characteristics (timing, memory, CPU use) as Verify.
		// Used to guard against side channel attacks when e.g. username was guessed incorrectly.
		static void DummyVerify(Seq pw);
	};
}
