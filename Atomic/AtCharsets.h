#pragma once

#include "AtSeq.h"

namespace At
{
	// Returns zero if charset name not recognized.
	// Charset names correspond to those used in .NET, and include preferred MIME names defined by IANA.
	UINT CharsetNameToWindowsCodePage(Seq charset);
}
