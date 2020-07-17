#include "AtIncludes.h"
#include "AtSeTranslate.h"

#include "AtEnsureStackTrace.h"


namespace At
{

	#define MAP_BEGIN(ARG)	switch (ARG) {
	#define MAP(CODE)		case CODE: return #CODE;
	#define MAP_END()		default: return "Unrecognized structured exception code"; }

	char const* DescribeStructuredExceptionCode(int64 code)
	{
		MAP_BEGIN(code)
		MAP(EXCEPTION_ACCESS_VIOLATION)
		MAP(EXCEPTION_BREAKPOINT)
		MAP(EXCEPTION_DATATYPE_MISALIGNMENT)
		MAP(EXCEPTION_SINGLE_STEP)
		MAP(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
		MAP(EXCEPTION_FLT_DENORMAL_OPERAND)
		MAP(EXCEPTION_FLT_DIVIDE_BY_ZERO)
		MAP(EXCEPTION_FLT_INEXACT_RESULT)
		MAP(EXCEPTION_FLT_INVALID_OPERATION)
		MAP(EXCEPTION_FLT_OVERFLOW)
		MAP(EXCEPTION_FLT_STACK_CHECK)
		MAP(EXCEPTION_FLT_UNDERFLOW)
		MAP(EXCEPTION_INT_DIVIDE_BY_ZERO)
		MAP(EXCEPTION_INT_OVERFLOW)
		MAP(EXCEPTION_PRIV_INSTRUCTION)
		MAP(EXCEPTION_NONCONTINUABLE_EXCEPTION)
		MAP(EXCEPTION_IN_PAGE_ERROR)
		MAP(EXCEPTION_ILLEGAL_INSTRUCTION)
		MAP(EXCEPTION_STACK_OVERFLOW)
		MAP(EXCEPTION_INVALID_DISPOSITION)
		MAP(EXCEPTION_GUARD_PAGE)
		MAP(EXCEPTION_INVALID_HANDLE)
		MAP_END()
	}

	#undef MAP_BEGIN
	#undef MAP
	#undef MAP_END



	void SeTranslator(uint code, _EXCEPTION_POINTERS* p)
	{
		// Note that SeTranslator is NOT called at the point of exception, but rather after stack has already unwound to the nearest exception handler.
		// For this reason, a stack trace performed at this point is not very useful. However, we can identify where the exception occurred from the exception context.

		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:
		{
			if (!p)                                                   return;		// Must have exception pointers; otherwise, do not handle
			if (!p->ExceptionRecord)                                  return;		// Must have exception record; otherwise, do not handle
			if (p->ExceptionRecord->NumberParameters < 2)             return;		// Must have parameters; otherwise, do not handle
			if (p->ExceptionRecord->ExceptionInformation[1] > 0x3FFF) return;		// Must be null pointer dereference. Do not handle exception if program tries to access a random address
																					// Null pointer assignment partition is up to 0xFFFF. To be safer, we tolerate just the first quarter of it
			
			EnsureFailDescRef efdRef { EnsureFailDesc::Create() };
			if (p->ExceptionRecord->ExceptionInformation[0])
			{
				efdRef.Add("Null pointer write\r\n");
				Ensure_AddExceptionContext(efdRef, p);
				throw SE_NullPointerWrite(std::move(efdRef));
			}
			else
			{
				efdRef.Add("Null pointer read\r\n");
				Ensure_AddExceptionContext(efdRef, p);
				throw SE_NullPointerRead(std::move(efdRef));
			}
		}

		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_GUARD_PAGE:
			return;

		default:
			EnsureFailDescRef efdRef { EnsureFailDesc::Create() };
			efdRef.Add(DescribeStructuredExceptionCode(code)).Add("\r\n");
			Ensure_AddExceptionContext(efdRef, p);
			throw StructuredException(code, std::move(efdRef));
		}
	}

}
