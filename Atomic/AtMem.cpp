#include "AtIncludes.h"
#include "AtMem.h"

// The below global objects MUST be initialized before code that may call the allocation functions in AtMem.h
#pragma warning (push)
#pragma warning (disable: 4073)		// L3: initializers put in library initialization area
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	HANDLE Mem::s_processHeap { GetProcessHeap() };

	__declspec(noinline) void Mem::BadAlloc()
	{
		if (IsDebuggerPresent())
			DebugBreak();
		
		throw std::bad_alloc();
	}

}
