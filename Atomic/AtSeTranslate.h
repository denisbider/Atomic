#pragma once

#include "AtException.h"
#include "AtEnsureFailDesc.h"


namespace At
{
	char const* DescribeStructuredExceptionCode(int64 code);


	struct StructuredException : Exception
	{
		StructuredException(int64 code, EnsureFailDescRef&& efdRef) noexcept
			: m_code(code), m_efdRef(std::move(efdRef)) {}

		char const* what() const override final { return m_efdRef.Z(); }
		int64 Code() const { return m_code; }

	private:
		int64             m_code;
		EnsureFailDescRef m_efdRef;
	};


	struct SE_NullPointerAccess : StructuredException
	{
		SE_NullPointerAccess(EnsureFailDescRef&& efdRef) noexcept
			: StructuredException(EXCEPTION_ACCESS_VIOLATION, std::move(efdRef)) {}
	};

	struct SE_NullPointerRead  : SE_NullPointerAccess { SE_NullPointerRead  (EnsureFailDescRef&& efdRef) noexcept : SE_NullPointerAccess(std::move(efdRef)) {} };
	struct SE_NullPointerWrite : SE_NullPointerAccess { SE_NullPointerWrite (EnsureFailDescRef&& efdRef) noexcept : SE_NullPointerAccess(std::move(efdRef)) {} };


	void SeTranslator(uint code, _EXCEPTION_POINTERS* p);

}
