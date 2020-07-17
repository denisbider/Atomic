#pragma once

#include "AtException.h"
#include "AtSlice.h"


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

		Args& ConvertAll() { while (Any()) Next(); return *this; }
		Slice<Seq> Converted() { return m_convertedSeqs; }

	private:
		sizet m_argc {};
		wchar_t const* const* m_argv {};
		Vec<Str> m_converted;
		Vec<Seq> m_convertedSeqs;
	};

}
