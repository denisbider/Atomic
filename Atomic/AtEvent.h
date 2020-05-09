#pragma once

#include "AtException.h"
#include "AtRp.h"
#include "AtWinErr.h"


namespace At
{

	class Event : NoCopy
	{
	public:
		enum ECreateAuto { CreateAuto };
		enum ECreateManual { CreateManual };

		Event() {}
		Event(Event&& x) noexcept : m_h(x.m_h) { x.m_h = 0; }
		Event(ECreateAuto) { Create(CreateAuto); }
		Event(ECreateManual) { Create(CreateManual); }
		Event(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initState, LPCTSTR name) { Create(sa, manual, initState, name); }

		~Event() noexcept { Close(); }
		void Close() noexcept;

		Event& operator= (Event&& x) noexcept { Close(); m_h = x.m_h; x.m_h = 0; return *this; }

		bool IsValid() const { return m_h != 0; }

		void Create(ECreateAuto) { Create(0, false, false, 0); }
		void Create(ECreateManual) { Create(0, true, false, 0); }
		void Create(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initState, LPCTSTR name);

		HANDLE Handle         () const             { return m_h; }
		void   Signal         ()                   { Signal(m_h); }
		void   ClearSignal    ()                   { ClearSignal(m_h); }
		bool   Wait           (DWORD waitMs) const { return Wait(m_h, waitMs); }
		void   WaitIndefinite () const             { return WaitIndefinite(m_h); }
		bool   IsSignaled     () const             { return IsSignaled(m_h); }

		static void Signal         (HANDLE h);
		static void ClearSignal    (HANDLE h);
		static bool Wait           (HANDLE h, DWORD waitMs);
		static void WaitIndefinite (HANDLE h);
		static bool IsSignaled     (HANDLE h) { return Wait(h, 0); }

	private:
		HANDLE m_h {};
	};


	class SharedEvent : public RefCountable, public Event
	{
	public:
		SharedEvent() {}
		SharedEvent(SharedEvent&& x) noexcept : Event((Event&&) x) {}
		SharedEvent(ECreateAuto) : Event(CreateAuto) {}
		SharedEvent(ECreateManual) : Event(CreateManual) {}
		SharedEvent(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initState, LPCTSTR name) : Event(sa, manual, initState, name) {}
	};

}
