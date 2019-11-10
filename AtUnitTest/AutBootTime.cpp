#include "AutIncludes.h"
#include "AutMain.h"


void ShowProcessStartTime(DWORD pid)
{
	Console::Out(Str("Trying PID ").UInt(pid).Add("\r\n"));

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess)
	{
		DWORD err = GetLastError();
		Console::Out(Str("Error in OpenProcess: ").Fun(DescribeWinErr, err).Add("\r\n"));
	}
	else
	{
		OnExit closeProcess = [&] { CloseHandle(hProcess); };

		FILETIME creationTime {}, exitTime {}, kernelTime {}, userTime {};
		if (!GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
		{
			DWORD err = GetLastError();
			Console::Out(Str("Error in GetProcessTimes: ").Fun(DescribeWinErr, err).Add("\r\n"));
		}
		else
		{
			Console::Out(Str("Cre. time: ").Obj(Time::FromFt(creationTime), TimeFmt::IsoMicro).Add("\r\n"));
		}
	}
}


void MeasureBootTime_TickCount()
{
	auto makeMeasurement = [] () -> Time
		{
			Sleep(0);
			Time now = Time::NonStrictNow();
			uint64 tc = GetTickCount64();
			return now - Time::FromMilliseconds(tc);
		};

	{
		Time estBootTime = makeMeasurement();
		Console::Out(Str("Est. time: ").Obj(estBootTime, TimeFmt::IsoMicro).Add("\r\n"));

		int64 cumulErr {};
		int64 nrMeasures {};
		int64 lastAvgErr {};
		int64 streakLen {};
		int64 longestStreak {};
		int64 nrStreaks10 {};
		int64 nrStreaks100 {};
		int64 nrStreaks1000 {};
		int64 nrStreaks10000 {};

		for (nrMeasures=1; nrMeasures!=10000000; ++nrMeasures)
		{
			Time measurement = makeMeasurement();
			cumulErr += ((int64) measurement.ToFt()) - ((int64) estBootTime.ToFt());
			int64 avgErr = cumulErr / nrMeasures;
			if (lastAvgErr != avgErr)
			{
				lastAvgErr = avgErr;
				streakLen = 0;
			}
			else
			{
				++streakLen;
				if (longestStreak < streakLen)
					longestStreak = streakLen;


				     if (streakLen  ==   10) ++nrStreaks10;
				else if (streakLen  ==  100) ++nrStreaks100;
				else if (streakLen ==  1000) ++nrStreaks1000;
				else if (streakLen == 10000) ++nrStreaks10000;
			}
		}

		int64 avgBootTimeInt = ((int64) estBootTime.ToFt()) + lastAvgErr;
		Time avgBootTime = Time::FromFt((uint64) avgBootTimeInt);

		Console::Out(Str("Longest streak:  ").SInt(longestStreak).Add("\r\n")
					.Add("Last streak len: ").SInt(streakLen).Add("\r\n")
					.Add("Nr streaks 10: ").SInt(nrStreaks10).Add(", 100: ").SInt(nrStreaks100).Add(", 1000: ").SInt(nrStreaks1000).Add(", 10000: ").SInt(nrStreaks10000).Add("\r\n")
					.Add("Avg. time: ").Obj(avgBootTime, TimeFmt::IsoMicro).Add("\r\n"));
	}
}


struct ActualSystemTimeOfDayInformation // Size=48
{
    LARGE_INTEGER BootTime; // Size=8 Offset=0
    LARGE_INTEGER CurrentTime; // Size=8 Offset=8
    LARGE_INTEGER TimeZoneBias; // Size=8 Offset=16
    ULONG TimeZoneId; // Size=4 Offset=24
    ULONG Reserved; // Size=4 Offset=28
    ULONGLONG BootTimeBias; // Size=8 Offset=32
    ULONGLONG SleepTimeBias; // Size=8 Offset=40
};


void BootTime()
{
	Console::Out("Using undocumented NtQuerySystemInformation:\r\n");

	{
		ActualSystemTimeOfDayInformation stodi {};
		NTSTATUS st = NtQuerySystemInformation(SystemTimeOfDayInformation, &stodi, sizeof(stodi), nullptr);
		if (st != STATUS_SUCCESS)
			Console::Out(Str("Error in NtQuerySystemInformation: ").Fun(DescribeNtStatus, st).Add("\r\n"));
		else
		{
			Time bootTime = Time::FromFt((uint64) stodi.BootTime.QuadPart);
			Time withBias = Time::FromFt((uint64) (stodi.BootTime.QuadPart + stodi.BootTimeBias));
			Console::Out(Str("NtQuerySystemInformation:\r\n")
				.Add("Boot time: ").Obj(bootTime, TimeFmt::IsoMicro).Add("\r\n")
				.Add("With bias: ").Obj(withBias, TimeFmt::IsoMicro).Add("\r\n")
				.Add("Bias: ").SInt((int64) stodi.BootTimeBias).Add("\r\n"));

			// Windows 8.1, tested 2016-09-06:
			// - BootTime appears to be valid, and appears to be in UTC
			// - Value is consistent across process invocations
			// - Session does not have to be elevated to get boot time
			// - BootTime appears to be 1 minute before the time returned by (Get-WmiObject Win32_OperatingSystem).LastBootUpTime, making it more authoritative
			// - BootTimeBias appears to have a nonsense value (if interpreted as FILETIME, an amount of -60.5 seconds or so)
			// - No change to this time after a short sleep and resume.
		}
	}

	Console::Out("\r\n"
				 "Using GetProcessTimes:\r\n");

	{
		ShowProcessStartTime(4);

		// Windows 8.1, tested 2016-09-06:
		// - PID 0 (System Idle Process) fails - "Windows error 87: The parameter is incorrect."
		// - PID 4 (System) succeeds in elevated process, fails in non-elevated with Access denied.
		// - Displayed process creation time for PID 4 (System) is consistent between process invocations.
		// - Creation time for PID 4 is a full 68 seconds after time from NtQuerySystemInformation, or time estimated using GetTickCount64.
		// - Creation time for PID 4 is also about 7 seconds after the time returned by (Get-WmiObject Win32_OperatingSystem).LastBootUpTime.
		// - No change to this time after a short sleep and resume.
	}

	Console::Out("\r\n"
				 "Using GetTickCount64:\r\n");

	{
		MeasureBootTime_TickCount();

		// Windows 8.1, tested 2016-09-06:
		// - Estimates a boot time about 250 ms earlier than that obtained from NtQuerySystemInformation
		// - Calculated values are different each time, but fall within a 15 ms (15,000 microsecond) range when the system is not under stress
		// - Averaged out over 1,000,000 measurements, the average estimated boot time still varies within about 100 microseconds.
		// - Averaged out over 10,000,000 measurements, it varies within about 30 microseconds.
		// - This was in debug mode, with the system under negligible load. No idea how much it might vary under load.
		// - Since standard error goes down by 1/sqrt(N), the number of measurements to get error below 1 microsecond is probably around 1 billion.
		// - Average measurements shifted toward earlier by 40-50 microseconds after a short sleep and resume. Could also be due to a constant drift.
	}
}
