#include "AtIncludes.h"
#include "AtTagStack.h"

namespace At
{

	Seq TagStack::Last() const
	{
		sizet i { m_tags.Len() };
		while (i > 0)
		{
			--i;
			if (m_tags[i] == '/')
				return Seq(&m_tags[i+1], m_tags.Len() - (i+1));
		}	
		return Seq();
	}


	bool TagStack::Contains(Seq tag) const
	{
		Seq reader = m_tags;
		reader.StripPrefixExact("/");

		while (reader.n)
		{
			Seq stackTag = reader.ReadToByte('/');
			if (stackTag.EqualInsensitive(tag))
				return true;

			reader.DropByte();
		}

		return false;
	}


	void TagStack::PopTagMismatch(Seq expected, Seq actual)
	{
		Str msg;
		msg.SetAdd("Pop tag mismatch - expected: ", expected, ", actual: ", actual, Seq("\0", 1));
		EnsureFailWithDesc(OnFail::Throw, msg.CharPtr(), __FUNCTION__, __LINE__);
	}

}
