#pragma once

#include "AtIncludes.h"
#include "AtStr.h"

namespace At
{
	// Intentionally does not inherit from Exception.
	// Requires special handling. Should not be treated as an arbitrary exception.
	struct ExecutionAborted {};


	struct NotImplemented : public Exception
		{ char const* what() const override { return "Not implemented"; } };


	struct StrErr : public Exception
	{
		StrErr(Seq msg) : m_s(Str::NullTerminate(msg)) {}	
		char const* what() const override { return m_s.CharPtr(); }
		Str const& S() const { return m_s; }
	protected:
		Str m_s;		// Null-terminated
	};

	struct InputErr : public StrErr { InputErr(Seq msg) : StrErr(msg) {} };
	struct UsageErr : public StrErr { UsageErr(Seq msg) : StrErr(msg) {} };


	struct ZLitErr : public Exception
	{
		ZLitErr(char const* z) : m_z(z) {}
		char const* what() const override { return m_z; }
	
	protected:
		char const* m_z;
	};


	struct ErrCode
	{
		ErrCode(int64 code) : m_code(code) {}
		int64 m_code;
	};

	template <class StrErrOrDerived = StrErr>
	struct ErrWithCode : public StrErrOrDerived, public ErrCode
	{
		ErrWithCode(int64 code, Seq msg) : StrErrOrDerived(msg), ErrCode(code) {}

		char const* what() const override
		{
			if (!m_built.Any())
			{
				m_built.Set(Seq(m_s).DropLastByte());
				BuildDesc(m_built);
				m_built.Byte(0);
			}

			return m_built.CharPtr();
		}

		virtual void BuildDesc(Str& s) const { s.Add(": ").ErrCode(m_code); }

	private:
		mutable Str m_built;	// Null-terminated if non-empty
	};
}
