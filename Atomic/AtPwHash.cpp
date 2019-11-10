#include "AtIncludes.h"
#include "AtPwHash.h"

#include "AtAuto.h"
#include "AtCrypt.h"

namespace At
{
	void PwHash::Generate(Seq pw, byte* result)
	{
		result[0] = 0xBB;
		result[1] = (byte) ((((uint32) BB_NrOps) >> 24) & 0xff);
		result[2] = (byte) ((((uint32) BB_NrOps) >> 16) & 0xff);
		result[3] = (byte) ((((uint32) BB_NrOps) >>  8) & 0xff);
		result[4] = (byte) ((((uint32) BB_NrOps)      ) & 0xff);
		result[5] = BB_NonceLen;

		byte* noncePtr { result + BB_HdrLen };
		Crypt::GenRandom(noncePtr, BB_NonceLen);
		Seq nonce(noncePtr, BB_NonceLen);

		byte* digestPtr { noncePtr + BB_NonceLen };
		AutoFree<BusyBeaver<BB_SHA512>> bb { new BusyBeaver<BB_SHA512>(BB_NrOps) };
		bb->Process(result, BB_SaltLen, pw.p, NumCast<uint>(pw.n), digestPtr, nullptr);
	}


	bool PwHash::Verify(Seq pw, Seq pwHash)
	{
		// If changing this function, remember to update DummyVerify so it continues to match
		// the side channel characteristics of Verify (e.g. memory access, CPU use, timing).

		if (pwHash.n != Len)
			return false;

		if (pwHash.p[0] != 0xBB)
			return false;

		uint32 nrOpsUsed =
			(((uint32) pwHash.p[1]) << 24) |
			(((uint32) pwHash.p[2]) << 16) |
			(((uint32) pwHash.p[3]) <<  8) |
			(((uint32) pwHash.p[4])      );
		if (nrOpsUsed != BB_NrOps)
			return false;

		uint nonceLenUsed = pwHash.p[5];
		if (nonceLenUsed != BB_NonceLen)
			return false;

		Seq expectedHash(pwHash.p + BB_SaltLen, BusyBeaver<BB_SHA512>::OutputSize);
		Str digest { BusyBeaver<BB_SHA512>::OutputSize };

		AutoFree<BusyBeaver<BB_SHA512>> bb { new BusyBeaver<BB_SHA512>(nrOpsUsed) };
		bb->Process(pwHash.p, BB_SaltLen, pw.p, NumCast<uint>(pw.n), digest.Ptr(), nullptr);

		return expectedHash.EqualExact(digest);
	}


	void PwHash::DummyVerify(Seq pw)
	{
		byte salt[BB_SaltLen] = {};
		Str digest { BusyBeaver<BB_SHA512>::OutputSize };

		AutoFree<BusyBeaver<BB_SHA512>> bb { new BusyBeaver<BB_SHA512>(BB_NrOps) };
		bb->Process(salt, BB_SaltLen, pw.p, NumCast<uint>(pw.n), digest.Ptr(), nullptr);
	}
}
