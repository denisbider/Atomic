#include "AtIncludes.h"
#include "AtObjId.h"


namespace At
{

	ObjId ObjId::None(0, 0);
	ObjId ObjId::Root(1, 0);


	byte* ObjId::EncodeBin_Ptr(byte* p) const
	{
		memcpy(p, &m_index,    8); p += 8;
		memcpy(p, &m_uniqueId, 8); p += 8;
		return p;
	}


	void ObjId::EncodeBin(Enc& enc) const
	{
		Enc::Write write = enc.IncWrite(16);
		memcpy(write.Ptr(),   &m_index,    8);
		memcpy(write.Ptr()+8, &m_uniqueId, 8);
		write.Add(16);
	}


	bool ObjId::DecodeBin(Seq& s)
	{
		if (s.n < 16)
			return false;

		memcpy(&m_index,    s.p,   8);
		memcpy(&m_uniqueId, s.p+8, 8);
		s.DropBytes(16);
		return true;
	}


	bool ObjId::SkipDecodeBin(Seq& s)
	{
		if (s.n < 16)
			return false;

		s.DropBytes(16);
		return true;
	}


	void ObjId::EncObj(Enc& s) const
	{
		     if (*this == ObjId::None) s.Add("[None]");
		else if (*this == ObjId::Root) s.Add("[Root]");
		else                           s.Add("[").UInt(m_uniqueId).Add(".").UInt(m_index).Add("]");
	}


	bool ObjId::ReadStr(Seq& s)
	{
		m_uniqueId = 0;
		m_index = 0;

		if (s.StartsWithInsensitive("[None]"))
		{
			s.DropBytes(6);
			return true;
		}

		if (s.StartsWithInsensitive("[Root]"))
		{
			m_uniqueId = 1;

			s.DropBytes(6);
			return true;
		}

		Seq reader { s };
		if (reader.ReadByte() == '[')
		{
			uint64 u = reader.ReadNrUInt64Dec();
			if (reader.ReadByte() == '.')
			{
				uint64 i = reader.ReadNrUInt64Dec();
				if (reader.ReadByte() == ']')
				{
					m_uniqueId = u;
					m_index = i;

					s = reader;
					return true;
				}
			}
		}

		return false;
	}

}
