#pragma once

#include "AtEnsureFailDesc.h"


namespace At
{

	void Ensure_AddStackTrace(EnsureFailDescRef& efdRef, ULONG framesToSkip, ULONG framesToCapture) noexcept;

	void Ensure_AddExceptionContext(EnsureFailDescRef& efdRef, _EXCEPTION_POINTERS* p) noexcept;

}
