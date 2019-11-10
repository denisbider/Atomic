#include "AtIncludes.h"
#include "AtCspBuilder.h"

#include "AtBaseXY.h"
#include "AtCrypt.h"


namespace At
{

	void CspBuilder::AdditiveParse(Seq policy)
	{
		char const* const cspWs = "\t\n\f\r ";

		Str dirNameLower;
		Seq reader = policy;
		while (reader.n)
		{
			Seq dirAndVal = reader.ReadToByte(';').TrimBytes(cspWs);
			reader.DropByte();

			if (dirAndVal.n)
			{
				Seq directiveName = dirAndVal.ReadToFirstByteOf(cspWs);
				dirNameLower.Clear().Lower(directiveName);
				while (dirAndVal.n)
				{
					dirAndVal.DropToFirstByteNotOf(cspWs);
					Seq value = dirAndVal.ReadToFirstByteOf(cspWs);
					if (value.n)
						AddDirectiveValue_Lower(dirNameLower, value);
				}
			}
		}
	}


	Seq CspBuilder::Final()
	{
		if (!m_finalized)
		{
			m_final.Clear();
			m_final.ReserveExact(m_estSizeBytes);

			for (Directive const& d : m_directives)
				if (d.m_values.Any())
				{
					m_final.IfAny("; ").Add(d.m_dirNameLower);
					for (Seq value : d.m_values)
						m_final.Ch(' ').Add(value);
				}

			m_finalized = true;
		}

		return m_final;
	}


	Str CspBuilder::CspHash(Seq s, Seq algPrefix, ALG_ID algId)
	{
		Str hash = Hash::HashOf(s, algId);

		Str r;
		r.ReserveExact(1 + algPrefix.n + Base64::EncodeBaseLen(hash.Len()) + 1);
		r.Ch('\'').Add(algPrefix);

		Base64::MimeEncode(hash, r, Base64::Padding::Yes, Base64::NewLines::None());
		r.Ch('\'');
		return r;
	}


	void CspBuilder::AddDirectiveValue_Lower(Seq dirNameLower, Seq value)
	{
		if (dirNameLower.n && value.n)
		{
			m_finalized = false;

			Directive* d = m_directives.FindPtr( [dirNameLower] (Directive& d) { return d.m_dirNameLower.EqualExact(dirNameLower); } );
			if (!d)
			{
				d = &m_directives.Add();
				d->m_dirNameLower = m_store.AddStr(dirNameLower);
			}

			if (!d->m_values.Contains(value))
			{
				if (!d->m_values.Any())
					m_estSizeBytes += d->m_dirNameLower.n + 2;
			
				m_estSizeBytes += 1 + value.n;
				d->m_values.Add(m_store.AddStr(value));
			}
		}
	}

}
