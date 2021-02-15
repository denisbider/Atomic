#include "AtIncludes.h"
#include "AtConsole.h"

#include "AtAuto.h"
#include "AtHandleReader.h"
#include "AtInitOnFirstUse.h"
#include "AtMutex.h"
#include "AtUnicode.h"
#include "AtUtf8.h"
#include "AtUtfWin.h"
#include "AtWinErr.h"


namespace At
{

	namespace
	{
		LONG volatile   a_initFlag {};
		HANDLE          a_handles[3];
		bool            a_isRedirected[3];
		UINT            a_redirectedCp;

		AutoFree<Mutex> a_readMx;
		Str             a_readByteBuf;
		Vec<wchar_t>    a_readWideBuf;
		Vec<uint>       a_readQueue;
		sizet           a_readQueueCharsRead {};

		AutoFree<Mutex> a_writeMx;			// If locking both readMx and writeMx, always lock readMx first
		Vec<wchar_t>    a_writeWideBuf;


		void Console_Init()
		{
			InitOnFirstUse(&a_initFlag, [] ()
				{
					a_readMx.Set(new Mutex);
					a_writeMx.Set(new Mutex);

					a_handles[0] = GetStdHandle(STD_INPUT_HANDLE);
					a_handles[1] = GetStdHandle(STD_OUTPUT_HANDLE);
					a_handles[2] = GetStdHandle(STD_ERROR_HANDLE);

					DWORD mode;
					a_isRedirected[0] = !GetConsoleMode(a_handles[0], &mode);
					a_isRedirected[1] = !GetConsoleMode(a_handles[1], &mode);
					a_isRedirected[2] = !GetConsoleMode(a_handles[2], &mode);
					
					a_redirectedCp = GetACP();
				} );
		}


		sizet ReadCharsFromQueue(sizet nrCharsMax, byte* buf, sizet nrBytesMax, uint delim = UINT_MAX)
		{
			sizet nrBytesRead {};

			while (nrCharsMax)
			{
				if (a_readQueueCharsRead == a_readQueue.Len())
					break;

				uint c { a_readQueue[a_readQueueCharsRead] };
				if (nrBytesMax < Utf8::CodePointEncodedBytes(c))
					break;

				++a_readQueueCharsRead;
				--nrCharsMax;

				uint n = Utf8::WriteCodePoint(buf, c);
				nrBytesRead += n;
				buf += n;
				nrBytesMax -= n;

				if (delim != UINT_MAX && c == delim)
					break;
			}

			if (a_readQueueCharsRead > a_readQueue.Len() / 2)
			{
				a_readQueue.Erase(0, a_readQueueCharsRead);
				a_readQueueCharsRead = 0;
			}

			return nrBytesRead;
		}


		void Decode_ReadByteBuf_Into_ReadQueue_Utf8()
		{
			a_readQueue.ReserveExact(a_readQueue.Len() + a_readByteBuf.Len());

			Seq reader(a_readByteBuf);
			byte const* pRemaining { reader.p };

			while (true)
			{
				uint c;
				Utf8::ReadResult::E readResult = Utf8::ReadCodePoint(reader, c);
				if (readResult == Utf8::ReadResult::NeedMore)
					break;

				pRemaining = reader.p;
				if (readResult != Utf8::ReadResult::OK)
					c = Unicode::ReplacementChar;

				a_readQueue.Add(c);
			}

			if (pRemaining != a_readByteBuf.Ptr())
			{
				sizet nrConsumed { (sizet) (pRemaining - a_readByteBuf.Ptr()) };
				sizet nrRemaining { a_readByteBuf.Len() - nrConsumed };
				memmove(a_readByteBuf.Ptr(), pRemaining, nrRemaining);
				a_readByteBuf.ResizeExact(nrRemaining);
			}
		}


		void Decode_ReadWideBuf_Into_ReadQueue()
		{
			a_readQueue.ReserveExact(a_readQueue.Len() + a_readWideBuf.Len());

			bool danglingSurrogate {};
			for (sizet i=0; i!=a_readWideBuf.Len(); ++i)
			{
				wchar_t w1 = a_readWideBuf[i];
				if (Unicode::IsHiSurrogate(w1))
				{
					if (i+1 == a_readWideBuf.Len())
					{
						danglingSurrogate = true;
						break;
					}

					wchar_t w2 = a_readWideBuf[i+1];
					if (Unicode::IsLoSurrogate(w2))
						a_readQueue.Add(Unicode::JoinSurrogates(w1, w2));
					else
						a_readQueue.Add(Unicode::ReplacementChar);

					++i;
				}
				else if (Unicode::IsLoSurrogate(w1))
				{
					if (i+1 == a_readWideBuf.Len())
					{
						danglingSurrogate = true;
						break;
					}

					wchar_t w2 = a_readWideBuf[i+1];
					if (Unicode::IsHiSurrogate(w2))
						a_readQueue.Add(Unicode::JoinSurrogates(w2, w1));
					else
						a_readQueue.Add(Unicode::ReplacementChar);

					++i;
				}
				else
				{
					if (Unicode::IsValidCodePoint(w1))
						a_readQueue.Add(w1);
					else
						a_readQueue.Add(Unicode::ReplacementChar);
				}
			}

			if (!danglingSurrogate)
				a_readWideBuf.Clear();
			else
			{
				wchar_t w = a_readWideBuf.Last();
				a_readWideBuf.Clear();
				a_readWideBuf.Add(w);
			}
		}


		struct ReplenishMode { enum E { Char, Line }; };

		void ReplenishReadQueue(Rp<StopCtl> const& stopCtl, sizet nrBytesHint, ReplenishMode::E mode)
		{
			DWORD dwBytesHint;
				 if (nrBytesHint <  1000) dwBytesHint =  1000;
			else if (nrBytesHint > 64000) dwBytesHint = 64000;
			else                          dwBytesHint = (DWORD) nrBytesHint;

			if (a_isRedirected[0])
			{
				// Read until we have at least one Unicode character, or end of stream
				HandleReader reader { a_handles[0] };
				reader.SetStopCtl(stopCtl);
				reader.SetReadSize(dwBytesHint);

				do
				{
					// Read bytes from file or pipe
					reader.Read( [] (Seq& data) -> Reader::ReadInstr
						{
							sizet origLen { a_readByteBuf.Len() };
							a_readByteBuf.ResizeAtLeast(origLen + data.n);
							memcpy_s(a_readByteBuf.Ptr() + origLen, a_readByteBuf.Len() - origLen, data.p, data.n);
							return Reader::ReadInstr::Done;
						} );

					// Decode into read queue
					if (a_redirectedCp == CP_UTF8)
						Decode_ReadByteBuf_Into_ReadQueue_Utf8();
					else
					{
						// This will not work excellently for MBCS code pages, but we're primarily concerned with UTF-8.
						// Reading UTF-16 from files also currently not supported.
						ToUtf16(a_readByteBuf, a_readWideBuf, a_redirectedCp);
						a_readByteBuf.Clear();
						Decode_ReadWideBuf_Into_ReadQueue();
					}
				}
				while (!a_readQueue.Any());
			}
			else
			{
				// Read until we have at least one Unicode character. No end of stream possibility
				if (mode == ReplenishMode::Char)
				{
					// Read single key input
					do
					{
						// Check if input is available. Use abortable wait if not
						while (true)
						{
							DWORD nrEventsAvailable {};
							if (!GetNumberOfConsoleInputEvents(a_handles[0], &nrEventsAvailable))
								{ LastWinErr e; throw e.Make<>("Console: ReplenishReadQueue: Error in GetNumberOfConsoleInputEvents"); }
							if (nrEventsAvailable)
								break;

							if (!stopCtl.Any())
								Wait1(a_handles[0], INFINITE);
							else
							{
								DWORD waitResult { Wait2(stopCtl->StopEvent().Handle(), a_handles[0], INFINITE) };
								if (waitResult == 0)
									throw ExecutionAborted();
							}
						}

						// Read input
						INPUT_RECORD inRec;
						DWORD        nrEventsRead {};

						if (!ReadConsoleInputW(a_handles[0], &inRec, 1, &nrEventsRead))
							{ LastWinErr e; throw e.Make<>("Console: ReplenishReadQueue: Error in ReadConsoleInput"); }
						EnsureThrow(nrEventsRead == 1);

						if (inRec.EventType == KEY_EVENT)
						{
							KEY_EVENT_RECORD const& ker { inRec.Event.KeyEvent };
							if (ker.bKeyDown && ker.wRepeatCount > 0 && ker.uChar.UnicodeChar != 0)
							{
								// Add translated Unicode character to wide char buffer
								sizet origLen { a_readWideBuf.Len() };
								a_readWideBuf.ResizeAtLeast(origLen + ker.wRepeatCount);
								
								wchar_t* pWrite { a_readWideBuf.Ptr() + origLen };
								for (uint i=0; i!=ker.wRepeatCount; ++i)
									*pWrite++ = ker.uChar.UnicodeChar;
							}
						}

						// Decode wide char buffer into read queue
						Decode_ReadWideBuf_Into_ReadQueue();
					}
					while (!a_readQueue.Any());
				}
				else
				{
					// Read line input
					if (stopCtl.Any())
						EnsureThrow(!"Console: ReplenishReadQueue: Abortable read not available for this input mode");

					do
					{
						// Read wide chars from console
						sizet origLen { a_readWideBuf.Len() };
						a_readWideBuf.ResizeAtLeast(origLen + dwBytesHint);
						wchar_t* pwcWrite { a_readWideBuf.Ptr() + origLen };

						DWORD nrCharsRead {};
						if (!ReadConsoleW(a_handles[0], pwcWrite, dwBytesHint, &nrCharsRead, 0))
							{ LastWinErr e; throw e.Make<>("Console: ReplenishReadQueue: Error in ReadConsole()"); }
					
						a_readWideBuf.ResizeExact(origLen + nrCharsRead);

						// Decode wide char buffer into read queue
						Decode_ReadWideBuf_Into_ReadQueue();
					}
					while (!a_readQueue.Any());
				}
			}
		}

	} // anon


	bool Console::Redirected(Device device)
	{
		EnsureThrow(ValidDevice(device));
		Console_Init();

		return a_isRedirected[device];
	}


	void Console::SetRedirectedCodePage(UINT cp)
	{
		Console_Init();

		Locker readLocker(a_readMx.Ref());
		Locker writeLocker(a_writeMx.Ref());

		     if (cp == CP_ACP)        a_redirectedCp = GetACP();
		else if (cp == CP_OEMCP)      a_redirectedCp = GetOEMCP();
		else if (cp == CP_THREAD_ACP) GetLocaleInfoW(GetThreadLocale(), LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, (wchar_t*) &a_redirectedCp, 2);
		else                          a_redirectedCp = cp;
	}


	sizet Console::Read(byte* buf, sizet nrBytesMax)
	{
		EnsureThrow(nrBytesMax >= Utf8::MaxBytesPerChar);
		Console_Init();

		Locker locker(a_readMx.Ref());

		sizet nrBytesRead { ReadCharsFromQueue(SIZE_MAX, buf, nrBytesMax) };
		if (!nrBytesRead)
		{
			ReplenishReadQueue(nullptr, nrBytesMax, ReplenishMode::Line);
			nrBytesRead = ReadCharsFromQueue(SIZE_MAX, buf, nrBytesMax);
		}

		return nrBytesRead;
	}


	sizet Console::ReadLine(byte* buf, sizet nrBytesMax)
	{
		EnsureThrow(nrBytesMax >= Utf8::MaxBytesPerChar);
		Console_Init();

		Locker locker(a_readMx.Ref());
		
		sizet nrBytesRead {};
		do
		{
			nrBytesRead += ReadCharsFromQueue(SIZE_MAX, buf + nrBytesRead, nrBytesMax - nrBytesRead, '\n');
			EnsureAbort(nrBytesRead <= nrBytesMax);

			if (!nrBytesRead)
				ReplenishReadQueue(nullptr, nrBytesMax, ReplenishMode::Line);
		}
		while (!nrBytesRead ||											// Continue to read as long as: we have not read any bytes; or
			   (buf[nrBytesRead-1] != '\n' &&							// ... the last byte read is not LF; and
			    nrBytesMax - nrBytesRead < Utf8::MaxBytesPerChar));		// ... there is room for at least one more UTF-8 character

		return nrBytesRead;
	}


	uint Console::ReadChar(Rp<StopCtl> const& stopCtl)
	{
		Console_Init();

		Locker locker(a_readMx.Ref());

		if (!a_readQueue.Any())
		{
			ReplenishReadQueue(stopCtl, 1, ReplenishMode::Char);
			if (!a_readQueue.Any())
				return UINT_MAX;
		}

		uint c { a_readQueue.First() };
		a_readQueue.PopFirst();
		return c;
	}


	void Console::Write(Device device, Seq text)
	{
		EnsureThrow(ValidOutputDevice(device));
		Console_Init();
		
		Locker locker(a_writeMx.Ref());

		if (Redirected(device))
		{
			Str textA;
			StrCvtCp(text, textA, CP_UTF8, a_redirectedCp, a_writeWideBuf);

			DWORD bytesToWrite { NumCast<DWORD>(textA.Len()) };
			DWORD bytesWritten {};
			if (!WriteFile(a_handles[device], textA.Ptr(), bytesToWrite, &bytesWritten, 0))
				{ LastWinErr e; throw e.Make<>("Console::Write: Error in WriteFile"); }
			if (bytesWritten != textA.Len())
				{ LastWinErr e; throw e.Make<>("Console::Write: Unexpected number of bytes written in WriteFile"); }
		}
		else
		{
			ToUtf16(text, a_writeWideBuf, CP_UTF8);

			// WriteConsole instead of cout or wcout provides better chances for Unicode characters to make it to screen properly
			wchar_t const* pchRemaining { a_writeWideBuf.Ptr() };
			DWORD cchRemaining { NumCast<DWORD>(a_writeWideBuf.Len()) };
			do
			{
				DWORD cchWritten {};
				if (!WriteConsoleW(a_handles[device], pchRemaining, cchRemaining, &cchWritten, 0))
				{
					DWORD rc { GetLastError() };
					if (rc != ERROR_NOT_ENOUGH_MEMORY || cchWritten == 0)
						throw WinErr<>(rc, "Console::Write: Error in WriteConsole()");
				}

				EnsureAbort(cchRemaining >= cchWritten);
				cchRemaining -= cchWritten;
			}
			while (cchRemaining != 0);
		}
	}


	void Console::Flush(Device device)
	{
		EnsureThrow(ValidOutputDevice(device));
		Console_Init();

		if (Redirected(device))
		{
			// According to community comments under this:
			//
			// MSDN: FlushFileBuffers function
			// https://msdn.microsoft.com/en-us/library/windows/desktop/aa364439.aspx
			//
			// ... some versions of Windows (7, 2008) have a bug where concurrent use of WriteFile and FlushFileBuffers
			// may cause the write to appear to succeed, but write corrupted data. Lock the mutex here to avoid this.

			Locker locker(a_writeMx.Ref());
			if (!FlushFileBuffers(a_handles[device]))
				{ LastWinErr e; throw e.Make<>("Console::Flush: Error in FlushFileBuffers"); }
		}
	}

}
