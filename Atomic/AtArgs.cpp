#include "AtIncludes.h"
#include "AtArgs.h"

#include "AtEnsure.h"
#include "AtUtfWin.h"


namespace At
{

	Args::Args(int argc, wchar_t const* const* argv)
	{
		if (argc < 0)
			throw Err("Invalid negative number of arguments");

		m_argc = (sizet) argc;
		m_argv = argv;
	}


	Args& Args::Skip(sizet n)
	{
		if (m_argc >= n)
		{
			m_argc -= n;
			m_argv += n;
		}

		return *this;
	}


	Seq Args::NextOrErr(Seq errDescIfNone)
	{
		if (!m_argc)
			throw Err(errDescIfNone);

		return Next();
	}


	Seq Args::Next()
	{
		EnsureThrow(0 != m_argc);

		m_converted.ReserveAtLeast(m_argc);
		Str& r = m_converted.Add();
		FromUtf16(*m_argv, -1, r, CP_UTF8);

		--m_argc;
		++m_argv;
		return r;
	}

}
