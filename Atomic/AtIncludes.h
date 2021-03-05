#pragma once

// Disable off-by-default warnings generated by header files when compiling with -Wall
#pragma warning (disable: 4091)  // L1: 'keyword': ignored on left of 'type' when no variable is declared
#pragma warning (disable: 4265)  // L4: 'class' : class has virtual functions, but destructor is not virtual
#pragma warning (disable: 4350)  // L4: behavior change: 'member1' called instead of 'member2'
#pragma warning (disable: 4365)  // L4: 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning (disable: 4571)  // L4: Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#pragma warning (disable: 4574)  // L4: 'identifier' is defined to be '0': did you mean to use '#if identifier'?
#pragma warning (disable: 4668)  // L4: 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning (disable: 4710)  // L4: 'function' : function not inlined
#pragma warning (disable: 4711)  // L4: function 'function' selected for inline expansion
#pragma warning (disable: 4820)  // L4: 'bytes' bytes padding added after construct 'member_name'


// Windows
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA
#define NOMINMAX
#define _CRT_RAND_S
#define SECURITY_WIN32
#define STRSAFE_NO_DEPRECATE	// Necessary to avoid errors in <cwchar>, <cstdio>, <cstring> when compiling with "v140_xp" build tools

#define WIN32_NO_STATUS
#include <winsock2.h>
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include <http.h>
#include <windns.h>
#include <mstcpip.h>
#include <wininet.h>
#include <sddl.h>
#include <security.h>
#include <schnlsp.h>
#include <strsafe.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <dbghelp.h>


// Macros that might require a newer Windows SDK version
#ifndef SP_PROT_TLS1_3_SERVER
#define SP_PROT_TLS1_3_SERVER 0x00001000
#endif

#ifndef SP_PROT_TLS1_3_CLIENT
#define SP_PROT_TLS1_3_CLIENT 0x00002000
#endif

#ifndef SP_PROT_TLS1_3
#define SP_PROT_TLS1_3 (SP_PROT_TLS1_3_SERVER | SP_PROT_TLS1_3_CLIENT)
#endif

#ifndef CALG_ECDH_EPHEM
#define CALG_ECDH_EPHEM 0x0000ae06
#endif


// Macros not defined when compiling with "v140_xp" build tools
#ifndef FILE_ATTRIBUTE_INTEGRITY_STREAM
#define FILE_ATTRIBUTE_INTEGRITY_STREAM 0x00008000
#endif

#ifndef FILE_ATTRIBUTE_NO_SCRUB_DATA
#define FILE_ATTRIBUTE_NO_SCRUB_DATA 0x00020000
#endif

#ifndef FILE_ATTRIBUTE_EA
#define FILE_ATTRIBUTE_EA 0x00040000
#endif

#ifndef FILE_FLAG_SESSION_AWARE
#define FILE_FLAG_SESSION_AWARE 0x00800000
#endif

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

#ifndef SCH_SEND_AUX_RECORD
#define SCH_SEND_AUX_RECORD 0x00200000
#endif

#ifndef SCH_USE_STRONG_CRYPTO
#define SCH_USE_STRONG_CRYPTO 0x00400000
#endif


// Macros we don't want
#ifdef SetPort
#undef SetPort
#endif


// std
#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stack>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <vector>

#include <eh.h>
#include <intrin.h>
#include <math.h>
#include <process.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>


// Off-by-default warnings at -W4 that I want on
#pragma warning (error: 4091)    // L1: 'keyword': ignored on left of 'type' when no variable is declared
#pragma warning (error: 4365)    // L4: 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning (error: 4574)    // L4: 'identifier' is defined to be '0': did you mean to use '#if identifier'?

// Off-by-default warnings at -W4 that I want off
#pragma warning (disable: 4061)  // L4: enumerator 'identifier' in a switch of enum 'enumeration' is not explicitly handled by a case label
#pragma warning (disable: 4371)  // L3: layout of class may have changed from a previous version of the compiler due to better packing of member 'member'
#pragma warning (disable: 4435)  // L4: 'class1' : Object layout under /vd2 will change due to virtual base 'class2'
#pragma warning (disable: 4437)  // L4: dynamic_cast from virtual base 'class1' to 'class2' could fail in some contexts Compile with /vd2 or define 'class2' with #pragma vtordisp(2) in effect
#pragma warning (disable: 4510)  // L4: ''': default constructor was implicitly defined as deleted
#pragma warning (disable: 4514)  // L4: 'function' : unreferenced inline function has been removed
#pragma warning (disable: 4582)  // L4: '': constructor is not implicitly called
#pragma warning (disable: 4583)  // L4: '': destructor is not implicitly called
#pragma warning (disable: 4625)  // L4: 'derived class': copy constructor could not be generated because a base class copy constructor is inaccessible
#pragma warning (disable: 4626)  // L4: 'derived class': assignment operator could not be generated because a base class assignment operator is inaccessible or deleted
#pragma warning (disable: 4868)  //     compiler may not enforce left-to-right evaluation order in braced initializer list -- Triggered falsely by single lambda, e.g. OnExit x { [] {} };
#pragma warning (disable: 5026)  // L4: 'class': move constructor was implicitly defined as deleted because a base class move constructor is inaccessible or deleted
#pragma warning (disable: 5027)  // L4: 'class': move assignment operator was implicitly defined as deleted because a base class move assignment operator is inaccessible or deleted

// On-by-default warnings at -W4 that I want off
#pragma warning (disable: 4127)  // L4: conditional expression is constant                                                 -- Constant conditional expressions are frequently useful in templates.
#pragma warning (disable: 4250)  // L2: 'class1' : inherits 'class2::member' via dominance                                 -- Dominance is virtually always what I want.
#pragma warning (disable: 4512)  // L4: 'class' : assignment operator could not be generated                               -- I would disable auto assignment operators and copy constructors if I could.
#pragma warning (disable: 4592)  // L4: '': : symbol will be dynamically initialized (implementation limitation)           -- VS 2015 Update 1 makes this go off unnecessarily. Re-enable when fixed
#pragma warning (disable: 4670)  // L4: 'identifier' : this base class is inaccessible                                     -- Accompanying message for C4673.
#pragma warning (disable: 4673)  // L4: throwing 'identifier' the following types will not be considered at the catch site -- Vec<> private base class not accessible in exception that derives from Str.


// The Visual Studio conditional (ternary) operator is dangerous. As specified by the language,
// and as implemented by GCC, it is fine. But compiling with VC only, it is treacherous. See:
//
// http://denisbider.blogspot.com/2015/06/c-ternary-operator-is-harmful.html
//
// In all but the most obvious situations, do not use the ?: operator AT ALL. Use the below lambda macro instead.
// It is efficient. Has minimal performance impact in Debug mode, and is even faster than ?: in Release mode.

#define If(EXPR, T, A, B) \
	(([&]() -> T { if (EXPR) return (A); return (B); })())

#define NoExcept(EXPR) \
	([&]() { static_assert(noexcept(EXPR), "Expression can throw"); }, (EXPR))


// Used to stringify at compile time other macros like __LINE__.
// The use of two macros ensures that any macro passed will be expanded before being stringified.

#define AT_EXPAND_STRINGIFY_IMPL(X) #X
#define AT_EXPAND_STRINGIFY(X) AT_EXPAND_STRINGIFY_IMPL(X)


namespace At
{
	enum class CanThrow  { No, Yes };
	enum class CaseMatch { Exact, Insensitive };
	enum class CharCase  { Upper, Lower };
	enum class EncrDir   { Encrypt, Decrypt };
	enum class EnumDir   { Forward, Reverse };
	enum class ProtoSide { None, Client, Server };
	enum class Normalize { No, Yes };
	enum class Enabled   { No, Yes };
	enum class Editable  { No, Yes };


	struct Exception         : public std::exception {};
	struct OutOfBounds       : public Exception { char const* what() const override { return "Value out of bounds"; } };
	struct OutOfFixedStorage : public Exception { char const* what() const override { return "Fixed storage exceeded"; } };


	class NoCopy
	{
	private:
		NoCopy(NoCopy const& x) = delete;
		void operator= (NoCopy const& x) = delete;
	public:
		NoCopy() noexcept = default;
		NoCopy(NoCopy&&) noexcept = default;
	};


	class OnExit : NoCopy
	{
	public:
		OnExit(std::function<void()> f) : m_f(f) {}
		~OnExit() { if (m_f) m_f(); }

		void Dismiss() { m_f = nullptr; }

	private:
		std::function<void()> m_f;
	};


	struct TypeIndex : std::type_index
	{
		// Wrapper around std::type_index that provides a default constructor (and a less clumsy name)

		struct None {};

		TypeIndex()                         noexcept : std::type_index(typeid(None)) {};
		TypeIndex(TypeIndex       const& x) noexcept : std::type_index(x)            {};
		TypeIndex(std::type_index const& x) noexcept : std::type_index(x)            {};
		TypeIndex(std::type_info  const& x) noexcept : std::type_index(x)            {};

		TypeIndex& operator= (TypeIndex       const& x) noexcept { std::type_index::operator=(x); return *this; }
		TypeIndex& operator= (std::type_index const& x) noexcept { std::type_index::operator=(x); return *this; }
	};

	static_assert(sizeof(TypeIndex) == sizeof(std::size_t), "TypeIndex is being used under the assumption it's lightweight");



#ifdef _M_X64

	static_assert(sizeof(ptrdiff_t) == sizeof(LONG64), "");

	__forceinline ptrdiff_t InterlockedIncrement_PtrDiff   (ptrdiff_t volatile* x)              { return InterlockedIncrement64   (x);                     }
	__forceinline ptrdiff_t InterlockedDecrement_PtrDiff   (ptrdiff_t volatile* x)              { return InterlockedDecrement64   (x);                     }
	__forceinline ptrdiff_t InterlockedExchangeAdd_PtrDiff (ptrdiff_t volatile* x, ptrdiff_t v) { return InterlockedExchangeAdd64 (x, v);                  }
		
#else

	static_assert(sizeof(ptrdiff_t) == sizeof(LONG), "");

	__forceinline ptrdiff_t InterlockedIncrement_PtrDiff   (ptrdiff_t volatile* x)              { return InterlockedIncrement     ((LONG volatile*) x);    }
	__forceinline ptrdiff_t InterlockedDecrement_PtrDiff   (ptrdiff_t volatile* x)              { return InterlockedDecrement     ((LONG volatile*) x);    }
	__forceinline ptrdiff_t InterlockedExchangeAdd_PtrDiff (ptrdiff_t volatile* x, ptrdiff_t v) { return InterlockedExchangeAdd   ((LONG volatile*) x, v); }

#endif

}
