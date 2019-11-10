#pragma once

#include "AtIncludes.h"
#include "AtEnc.h"


namespace At
{

	// Whitespace-packs JavaScript source code. Limitations:
	// - names of identifiers are left unchanged
	// - JS regular expressions will cause parsing errors or incorrect output
	// - code relying on semicolon insertion will break since newlines are removed
	void JsWsPack(Enc& enc, Seq content);

}
