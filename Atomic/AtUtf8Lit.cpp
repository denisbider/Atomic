#include "AtIncludes.h"
#include "AtUtf8Lit.h"


namespace At
{
	namespace Utf8
	{
		namespace Lit
		{
			Seq const BOM      { "\xEF\xBB\xBF", 3 };	// U+FEFF
			Seq const Ellipsis { "\xE2\x80\xA6", 3 };	// U+2026
		}
	}
}

