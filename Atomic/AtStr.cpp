#include "AtIncludes.h"
#include "AtStr.h"


namespace At
{

	bool StrSlice_Equal(Slice<Str> l, Slice<Str> r, CaseMatch cm)
	{
		if (l.Len() != r.Len())
			return false;

		for (sizet i=0; i!=l.Len(); ++i)
			if (!Seq(l[i]).Equal(r[i], cm))
				return false;

		return true;
	}

}
