#pragma once

#include "AtAbortable.h"
#include "AtException.h"
#include "AtStopCtl.h"
#include "AtStr.h"
#include "AtEvent.h"


namespace At
{

	void MSVC_SetThreadName(DWORD threadId, LPCSTR name);


	class Thread : public Abortable
	{
	public:
		enum ECreate { Create };

		virtual ~Thread() {}

	public:
		// IMPLEMENTED BY USER

		// Called from the parent thread before starting the child thread; may throw an exception if some crucial setting has not been set, etc
		virtual Str  ThreadDesc        ();
		virtual void BeforeThreadStart () {}

		// Can only set one of the stack size values. Default linker settings are used if size 0 is returned. 
		virtual uint StackCommitSize   () { return 0; }
		virtual uint StackReserveSize  () { return 0; }

		// Override this method to provide the code that will execute in the new thread.
		virtual void ThreadMain() = 0;

		// Override this to perform a special action when Run() finishes, regardless of
		// how it finishes. This might be used to, e.g., send a signal to another thread.
		struct ExitType { enum E { Ended, FailedToStart }; };
		virtual void OnThreadExit(ExitType::E) {}

	public:
		void AddRef  () { InterlockedIncrement(&m_refs); }
		void Release () { if (!InterlockedDecrement(&m_refs)) delete this; }

		// Implemented in ThreadFinal<> template rather than here to control
		// creation of thread objects (must not be created on the stack!)
		virtual Thread& Start() = 0;

		void Join() { AbortableWait(ThreadHandle()); }

		bool   Started      () const { return m_threadInfo->IsValid(); }
		HANDLE ThreadHandle () const { return m_threadInfo->Handle(); }

	protected:
		LONG volatile  m_refs       {};
		Rp<ThreadInfo> m_threadInfo { new ThreadInfo() };

		static uint __stdcall ThreadEnvelope (void* param);
		static void           StartThread    (Thread* obj);
	};


	template <class T>
	class ThreadPtr;


	template <class T>
	class ThreadFinal : public T
	{
	private:
		template <typename... Args>
		ThreadFinal(Args&&... args) : T(std::forward<Args>(args)...) {}

	public:
		// Do not call directly, use ThreadPtr<YourThreadClass>::Create()
		template <typename... Args>
		static void ThreadFinalCreateThread(ThreadPtr<T>& x, Args&&... args);

		// A thread must have a StopCtl configured before starting. This is asserted in StartThread().
		Thread& Start() override final { StartThread(this); return *this; }
		Thread& Start(Rp<StopCtl> const& sc) { SetStopCtl(sc); return Start(); }
	};


	template <class T>
	class ThreadPtr
	{
	public:
		ThreadPtr() {}
		ThreadPtr(ThreadPtr<T> const& x) { Detach(x.m_ptr); }

		template <typename... Args>
		ThreadPtr(Thread::ECreate, Args&&... args) { Create(std::forward<Args>(args)...); }

		~ThreadPtr() { if (m_ptr) m_ptr->Release(); }

		ThreadPtr<T>& operator= (ThreadPtr<T> const& x) { Detach(x.m_ptr); return *this; }

		template <typename... Args>
		void Create(Args&&... args) { ThreadFinal<T>::ThreadFinalCreateThread<Args...>(*this, std::forward<Args>(args)...); }

		void Reset() { Detach(nullptr); }

		void Detach(ThreadFinal<T>* ptr)
		{
			if (ptr != nullptr) ptr->AddRef();
			if (m_ptr != nullptr) m_ptr->Release();
			m_ptr = ptr;
		}

		ThreadFinal<T>& Ref()         const { EnsureThrow(m_ptr); return *m_ptr; }
		ThreadFinal<T>* Ptr()         const { return m_ptr; }
		ThreadFinal<T>* operator-> () const { return m_ptr; }

	private:
		ThreadFinal<T>* m_ptr {};
	};


	template <class T>
	template <typename... Args>
	void ThreadFinal<T>::ThreadFinalCreateThread(ThreadPtr<T>& x, Args&&... args)
	{
		x.Detach(new ThreadFinal<T>(std::forward<Args>(args)...));
	}

}
