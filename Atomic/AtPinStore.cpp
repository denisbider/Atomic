#include "AtIncludes.h"
#include "AtPinStore.h"


namespace At
{

	Enc& PinStore::GetEnc(sizet writeBytes)
	{
		if (m_strs.Any() && m_strs.Last().Remaining() >= writeBytes)
			return m_strs.Last();

		if (writeBytes < m_bytesPerStr / 2)
			return m_strs.Add().FixReserve(m_bytesPerStr);

		return m_largeStrs.Add().FixReserve(writeBytes);
	}


	Seq PinStore::AddStr(Seq s)
	{
		Enc& enc = GetEnc(s.n);
		Enc::Write write = enc.IncWrite(s.n);
		byte* pWrite = write.Ptr();
		memcpy(pWrite, s.p, s.n);
		write.Add(s.n);
		return Seq(pWrite, s.n);
	}


	Seq PinStore::AddStrFromParts(Slice<Seq> parts)
	{
		sizet n {};
		for (Seq const* s=parts.begin(); s!=parts.end(); ++s)
			n += s->n;

		Enc&       enc    = GetEnc(n);
		Enc::Write write  = enc.IncWrite(n);
		byte*      pStart = write.Ptr();
		byte*      pWrite = pStart;

		for (Seq const* s=parts.begin(); s!=parts.end(); ++s)
		{
			memcpy(pWrite, s->p, s->n);
			pWrite += s->n;
		}

		Seq result { pStart, write.AddUpTo(pWrite) };
		EnsureThrow(result.n == n);
		return result;
	}

}
