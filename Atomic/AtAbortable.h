#pragma once

#include "AtException.h"
#include "AtNum.h"
#include "AtStopCtl.h"
#include "AtWinErr.h"


namespace At
{

	class CommunicationErr : public StrErr
		{ public: CommunicationErr(Seq msg) : StrErr(msg) {} };


	struct Timeouts
	{
		bool   m_overallTimeout  {};
		uint64 m_expireTickCount {};

		void SetOverallExpireMs(uint64 ms);
	};


	class IAbortable
	{
	public:
		virtual ~IAbortable() noexcept {};

		virtual void SetStopCtl(Rp<StopCtl> const& sc) = 0;
		virtual void SetExpireTickCount(uint64 tickCount) = 0;
		
		void SetExpireMs(uint64 ms);
		void ApplyTimeouts(Timeouts const& timeouts, uint64 individualMs);
	};


	class Abortable : virtual public IAbortable, NoCopy
	{
	public:
		struct Err : CommunicationErr { Err(Seq msg) : CommunicationErr(msg) {} };
		struct TimeExpired : Err { TimeExpired() : Err("Time expired") {} };

		void        SetStopCtl (Rp<StopCtl> const& sc) override final { m_stopCtl = sc; }
		void        SetStopCtl (Abortable const& x) { m_stopCtl = x.m_stopCtl; }
		Rp<StopCtl> GetStopCtl () { return m_stopCtl; }
		Event&      StopEvent  () { return m_stopCtl->StopEvent(); }

		void SetExpireTickCount(uint64 tickCount) override final { m_expireTickCount = tickCount; }

		void SetAbortableParams(Abortable const& x) { m_stopCtl = x.m_stopCtl; m_expireTickCount = x.m_expireTickCount; }

	protected:
		Rp<StopCtl> m_stopCtl;
		uint64      m_expireTickCount { UINT64_MAX };

		void AbortableSleep(DWORD waitMs);
		void AbortableWait(HANDLE hEvent);
	};

}
