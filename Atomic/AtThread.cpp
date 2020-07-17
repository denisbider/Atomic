#include "AtIncludes.h"
#include "AtThread.h"

#include "AtNumCvt.h"


namespace At
{

	void MSVC_SetThreadName(DWORD threadId, LPCSTR name)
	{
		// Set thread name for Visual Studio. Based on:
		// https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

		DWORD const MsvcException = 0x406D1388;

		#pragma pack (push, 8)
		struct MSVC_THREADNAME_INFO
		{
			DWORD  dwType;		// Must be 0x1000.
			LPCSTR szName;		// Pointer to name (in user addr space).
			DWORD  dwThreadID;	// Thread ID (-1=caller thread).
			DWORD  dwFlags;		// Reserved for future use, must be zero.
   		};
		#pragma pack (pop)

		MSVC_THREADNAME_INFO info {};
		info.dwType     = 0x1000;
		info.szName     = name;
		info.dwThreadID = threadId;

		__try { RaiseException(MsvcException, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*) &info); }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}


	Str Thread::ThreadDesc()
	{
		Seq name { typeid(*this).name() };

		if (name.StripPrefixExact("class At::ThreadFinal<class ") ||
			name.StripPrefixExact("class At::ThreadFinal<struct "))
		{
			if (name.EndsWithExact(">"))
				name.RevDropByte();
		}

		return name;
	}


	uint __stdcall Thread::ThreadEnvelope(void* param)
	{
		_set_se_translator(SeTranslator);

		Thread* obj     { (Thread*) param };
		bool    runMain { true };

		try
		{
			if (IsDebuggerPresent())
			{
				Str threadDesc = Str::NullTerminate(obj->ThreadDesc());
				MSVC_SetThreadName((DWORD) -1, threadDesc.CharPtr());
			}
		}
		catch (std::exception const& e)
		{
			runMain = false;
			obj->m_stopCtl->Stop(Str("Thread: Exception while setting thread name: ").Add(e.what()));
		}

		if (runMain)
			try { obj->ThreadMain(); }
			catch (ExecutionAborted const&) { obj->m_stopCtl->Stop("Thread: Execution aborted"); }
			catch (std::exception const& e) { obj->m_stopCtl->Stop(Str("Thread: Exception in ThreadMain: ").Add(e.what())); }

		try { obj->OnThreadExit(Thread::ExitType::Ended); }
		catch (std::exception const& e) { obj->m_stopCtl->Stop(Str("Thread: Exception in ThreadExit: ").Add(e.what())); }

		obj->Release();
		_endthreadex(0);
		return 0;
	}


	void Thread::StartThread(Thread* obj)
	{
		EnsureThrow(obj->m_stopCtl.Any());
		EnsureThrow(!obj->m_threadInfo->IsValid());

		obj->m_threadInfo->m_threadDesc = obj->ThreadDesc();
		obj->BeforeThreadStart();

		// We have to use _beginthreadex() and _endthreadex() rather than the WIN APIs to avoid memory leaks in the C/C++ runtime library.
		// We are using _beginthreadex()+_endthreadex(), rather than _beginthread()+_endthread(), because the former do not close the thread handle.

		uint flags     { CREATE_SUSPENDED };
		uint stackSize { obj->StackCommitSize() };
	
		uint stackReserveSize { obj->StackReserveSize() };
		if (stackReserveSize && stackReserveSize >= stackSize)
		{
			flags |= STACK_SIZE_PARAM_IS_A_RESERVATION;
			stackSize = stackReserveSize;
		}

		obj->AddRef();					// Add reference to be held by new thread. Normally released in ThreadEnvelope

		uint threadId;
		HANDLE h { (HANDLE) _beginthreadex(0, stackSize, ThreadEnvelope, obj, flags, &threadId) };
		if (!h)
		{
			int rc { errno };

			obj->Release();				// Release new thread's reference. We still have our own

			try { obj->OnThreadExit(Thread::ExitType::FailedToStart); }
			catch (std::exception const& e) { obj->m_stopCtl->Stop(Str("Thread: Exception in ThreadExit: ").Add(e.what())); }

			throw ErrWithCode<>(rc, "Thread: Error in _beginthreadex");
		}
		else
		{
			// In a low-memory condition, StopCtl::AddThread may throw std::bad_alloc. Allowing the new thread to run in this situation
			// would require the program to be terminated, e.g. via EnsureAbort, because the program cannot properly wait for the thread to end.
			// To allow the program to recover from std::bad_alloc in this situation, we create the thread suspended. Then, if AddThread succeeds,
			// we allow the thread to run with ResumeThread. But if AddThread throws, we terminate the thread before it has started.

			obj->m_threadInfo->SetHandle(h);

			OnExit terminateThread { [&]
				{
					if (!TerminateThread(h, 0xDEADBEEF))
						EnsureAbortWithNr(!"Error in TerminateThread", GetLastError());

					obj->Release();		// Release new thread's reference. We still have our own
				} };

			obj->m_stopCtl->AddThread(obj->m_threadInfo);
			terminateThread.Dismiss();

			DWORD preResumeSuspendCount { ResumeThread(h) };
			if (preResumeSuspendCount == (DWORD) -1)
				EnsureAbortWithNr(!"Error in ResumeThread", GetLastError());
			EnsureAbort(preResumeSuspendCount == 1);
		}
	}

}
