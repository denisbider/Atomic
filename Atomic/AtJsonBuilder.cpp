#include "AtIncludes.h"
#include "AtJsonBuilder.h"


namespace At
{

	JsonBuilder& JsonBuilder::Member(Seq name)
	{
		Elem& last = m_elems.Last();
		EnsureThrow(last.Is(ElemType::Object));
		if (!last.m_anyChildren)
			last.m_anyChildren = true;
		else
			m_s.Ch(',');

		m_elems.Push(ElemType::Member);
		Json::EncodeString(m_s, name);
		m_s.Ch(':');
		return *this;
	}

	
	void JsonBuilder::OnValue()
	{
		Elem& last = m_elems.Last();
		if (last.Is(ElemType::Member))
		{
			m_elems.PopLast();
			return;
		}

		if (last.Is(ElemType::Array))
		{
			if (!last.m_anyChildren)
				last.m_anyChildren = true;
			else
				m_s.Ch(',');
			return;
		}

		if (last.Is(ElemType::Root))
		{
			EnsureThrow(!last.m_anyChildren);
			last.m_anyChildren = true;
			return;
		}

		if (last.Is(ElemType::Object))
			EnsureThrow(!"Must add member before adding value to object");

		EnsureThrow(!"Unrecognized element type");
	}

}
