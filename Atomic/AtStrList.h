#pragma once

#include "AtStr.h"


namespace At
{

	Vec<Seq> VecStrToSeq(Vec<Str> const& x);
	Vec<Str> VecSeqToStr(Vec<Seq> const& x);

	void StrListCompare(Vec<Seq> const& oldEntries, Vec<Seq> const& newEntries, Vec<Seq>& entriesRemoved, Vec<Seq>& entriesAdded);

}
