#include "AtIncludes.h"
#include "AtToolhelp.h"

#include "AtVec.h"


namespace At
{

/*

// Pass 0 as the targetProcessId to suspend threads in the current process
void DoSuspendThread(DWORD targetProcessId, DWORD targetThreadId)
{
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(h, &te))
        {
            do
            {
                if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID)) 
                {
                    // Suspend all threads EXCEPT the one we want to keep running
                    if(te.th32ThreadID != targetThreadId && te.th32OwnerProcessID == targetProcessId)
                    {
                        HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                        if(thread != NULL)
                        {
                            SuspendThread(thread);
                            CloseHandle(thread);
                        }
                    }
                }
                te.dwSize = sizeof(te);
            } while (Thread32Next(h, &te));
        }
        CloseHandle(h);    
    }

}*/

	namespace
	{

		bool SuspendOtherThreads(Vec<DWORD>& threadIdsSeen)
		{
			bool anyNewThreadsSeen {};

			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (hSnapshot != INVALID_HANDLE_VALUE)
			{
				OnExit closeSnapshot = [hSnapshot] { CloseHandle(hSnapshot); };
				
				THREADENTRY32 te;
				te.dwSize = sizeof(te);
				if (Thread32First(hSnapshot, &te))
					do
					{
						if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
						{
							if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != GetCurrentThreadId())
							{
								if (!threadIdsSeen.Contains(te.th32ThreadID))
								{
									threadIdsSeen.Add(te.th32ThreadID);
									anyNewThreadsSeen = true;

									HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, te.th32ThreadID);
									if (hThread)
									{
										SuspendThread(hThread);
										CloseHandle(hThread);
									}
								}
							}
						}
					}
					while (Thread32Next(hSnapshot, &te));
			}

			return anyNewThreadsSeen;
		}

	}	// anon


	void SuspendAllOtherThreads()
	{
		Vec<DWORD> threadIds;
		while (SuspendOtherThreads(threadIds)) {}
	}

}
