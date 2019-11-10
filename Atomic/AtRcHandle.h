#pragma once

#include "AtException.h"
#include "AtRp.h"


namespace At
{

	class RcHandle : public RefCountable, NoCopy
	{
	public:
		RcHandle  () = default;
		RcHandle  (HANDLE handle) : m_handle(handle) {}
		~RcHandle ();

		bool   IsValid   () const        { return m_handle != 0 && m_handle != INVALID_HANDLE_VALUE; }
		void   SetHandle (HANDLE handle) { EnsureAbort(!IsValid()); m_handle = handle; }
		HANDLE Handle    () const        { return m_handle; }

	private:
		HANDLE m_handle { nullptr };
	};

}
