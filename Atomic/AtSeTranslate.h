#pragma once

#include "AtException.h"


namespace At
{
	char const* DescribeStructuredException(int64 code);


	struct StructuredException : public Exception
	{
		StructuredException(int64 code) : m_code(code) {}
		char const* what() const { return DescribeStructuredException(m_code); }

		int64 m_code;
	};


	struct SE_NullPointerAccess : public StructuredException
	{
		SE_NullPointerAccess() : StructuredException(EXCEPTION_ACCESS_VIOLATION) {}
		char const* what() const { return "Null pointer access"; }
	};

	struct SE_NullPointerRead  : public SE_NullPointerAccess { char const* what() const { return "Null pointer read";  } };
	struct SE_NullPointerWrite : public SE_NullPointerAccess { char const* what() const { return "Null pointer write"; } };


	void SeTranslator(uint code, _EXCEPTION_POINTERS* p);

}
