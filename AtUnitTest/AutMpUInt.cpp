#include "AutIncludes.h"
#include "AutMain.h"


void MpUIntTests()
{
	try
	{
		MpUInt m;
		for (unsigned int i=0; i!=1000; ++i) m.Inc();
		if (m != 1000U)											throw ZLitErr("m should be 1000");
		for (unsigned int i=0; i!=1000; ++i) m.Dec();
		if (!m.IsZero())										throw ZLitErr("m should be zero");

		enum : uint32 { CloseToMax = std::numeric_limits<uint32>::max() - 500 };
		m.Set(CloseToMax);
		if (m.NrBytes() != 4)									throw ZLitErr("m.NrBytes() should be 4");
		for (unsigned int i=0; i!=1000; ++i) m.Inc();
		if (m.NrBytes() != 5)									throw ZLitErr("m.NrBytes() should be 5");
		for (unsigned int i=0; i!=1000; ++i) m.Dec();
		if (m != MpUInt(CloseToMax))							throw ZLitErr("m should equal CloseToMax");
		if (m.NrBytes() != 4)									throw ZLitErr("m.NrBytes() should be 4 again");

		MpUInt one { 1 };
		MpUInt p256 = one.Shl(256).SubM(one.Shl(224)).AddM(one.Shl(192)).AddM(one.Shl(96)).SubM(one);

		Str p256Str;
		p256.WriteBytes(p256Str);
		Console::Out(Str("p256:  ").Hex(p256Str).Add("\r\n"));

		Seq p256Seq { "\xff\xff\xff\xff\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 32 };
		if (!p256Seq.EqualExact(p256Str))						throw ZLitErr("p256 does not encode as expected");

		MpUInt p256Other;
		p256Other.ReadBytes(p256Seq);

		if (p256 != p256Other)									throw ZLitErr("p256 does not compare as expected");
		if (p256.AddM(one) <= p256Other)						throw ZLitErr("p256+1 does not compare as expected");
		if (p256.SubM(one) >= p256Other)						throw ZLitErr("p256-1 does not compare as expected");
		if (p256.AddM(one).SubM(one) != p256Other)				throw ZLitErr("p256+1-1 does not compare as expected");
		if (p256.SubM(one).AddM(one) != p256Other)				throw ZLitErr("p256-1+1 does not compare as expected");

		for (unsigned int i=0; i!=1000; ++i)
			if (p256.Shl(i).Shr(i) != p256)						throw ZLitErr("((p256 << i) >> i) != p256");

		MpUInt ident;
		uint32 remainderS;
		p256.AddM(one).DivS(4, ident, remainderS);
		if (remainderS != 0)									throw ZLitErr("p256+1 expected to be divisible by 4");

		Str identStr;
		ident.WriteBytes(identStr);
		Console::Out(Str("ident: ").Hex(identStr).Add("\r\n"));

		Seq identSeq { "\x3f\xff\xff\xff\xc0\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 32 };
		if (!identSeq.EqualExact(identStr))						throw ZLitErr("ident does not encode as expected");

		{
			MpUInt q, r;
			p256.AddM(one).DivM(ident, q, r);
			if (!r.IsZero())									throw ZLitErr("p256+1 expected to be divisible by ident");
			if (q <= MpUInt(3))									throw ZLitErr("(p256+1)/ident expected to be more than 3");
			if (q != MpUInt(4))									throw ZLitErr("(p256+1)/ident expected to equal 4");
			if (q >= MpUInt(5))									throw ZLitErr("(p256+1)/ident expected to be less than 5");
		}

		{
			sizet nrDivsTested {};
			BCrypt::Rng rng;
			Str nBuf, dBuf;
		
			while (nrDivsTested != 1000)
			{
				uint32 nLen = rng.GenRandomNr32(127) + 1;
				rng.SetBufRandom(nBuf, nLen);
				rng.SetBufRandom(dBuf, rng.GenRandomNr32(nLen - 1) + 1);

				MpUInt n, d;
				n.ReadBytes(nBuf);
				d.ReadBytes(dBuf);

				if (!d.IsZero() && n > d)
				{
					MpUInt q, r;
					n.DivM(d, q, r);

					EnsureThrow(q <= n);
					EnsureThrow(r < d);

					MpUInt check;
					check = d.MulM(q).AddM(r);

					if (check != n)									throw ZLitErr("check should equal n");

					++nrDivsTested;
				}
			}

			Console::Out(Str().UInt(nrDivsTested).Add(" divs tested\r\n"));
		}

		{
			sizet nrKeysTested {};

			MpUInt yCmp;
			EllipticCurve nistp256 { EcParams::Nistp256 };
			BCrypt::EccPublicBlob ecBlob;
			BCrypt::Provider ecProv;
			ecProv.OpenEcdhP256();

			struct PreparedKey
			{
				BCrypt::Key m_key;
				MpUInt      m_x;
				MpUInt      m_y;
				bool        m_yOdd {};
			};

			enum { NrKeysToTest = 100 };
			Vec<PreparedKey> keys;
			keys.ReserveExact(NrKeysToTest);

			ULONGLONG startTicks = GetTickCount64();

			for (sizet i=0; i!=NrKeysToTest; ++i)
			{
				PreparedKey& pk = keys.Add();
				pk.m_key.GenerateKeyPair(ecProv, 0, 0);
				pk.m_key.FinalizeKeyPair();
				pk.m_key.Export(ecBlob.GetBuf(), BCRYPT_ECCPUBLIC_BLOB);
				pk.m_x.ReadBytes(ecBlob.Get_X());
				pk.m_y.ReadBytes(ecBlob.Get_Y());
				pk.m_yOdd = ((pk.m_y.GetByte(0) & 1) == 1);
			}

			ULONGLONG endTicks = GetTickCount64();
			Console::Out(Str().UInt(keys.Len()).Add(" EC keys generated in ").UInt(endTicks - startTicks).Add(" ms\r\n"));
			startTicks = GetTickCount64();

			for (PreparedKey const& pk : keys)
			{
				nistp256.CalculateY(pk.m_x, pk.m_yOdd, yCmp);
				EnsureThrow(yCmp == pk.m_y);
				++nrKeysTested;
			}

			endTicks = GetTickCount64();
			Console::Out(Str().UInt(nrKeysTested).Add(" points decompressed in ").UInt(endTicks - startTicks).Add(" ms\r\n"));
		}
	}
	catch (Exception const& e)
	{
		Str msg = "MpUIntTests terminated by exception:\r\n";
		msg.Add(e.what()).Add("\r\n");
		Console::Out(msg);
	}
}
