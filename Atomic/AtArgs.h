#pragma once

#include "AtException.h"


namespace At
{

	class Args
	{
	public:
		struct Err : StrErr { Err(Seq msg) : StrErr(msg) {} };

		Args(int   argc, wchar_t const* const* argv);
		Args(sizet argc, wchar_t const* const* argv) : m_argc(argc), m_argv(argv) {}

		bool Any() const { return 0 != m_argc; }
		sizet Len() const { return m_argc; }
		Args& Skip(sizet n);
		Seq NextOrErr(Seq errDescIfNone);
		Seq Next();

	private:
		sizet m_argc {};
		wchar_t const* const* m_argv {};
		Vec<Str> m_converted;
	};

}
