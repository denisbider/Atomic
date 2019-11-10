#pragma once

#include "AtStopCtl.h"
#include "AtStr.h"

namespace At
{
	// Implements writing to Windows console or file

	struct Console
	{
		enum Device { StdIn = 0, StdOut = 1, StdErr = 2 };
		static inline bool ValidDevice      (Device device) { return device <= StdErr; }
		static inline bool ValidOutputDevice(Device device) { return device == StdOut || device == StdErr; }

		static bool Redirected(Device device);
		static void SetRedirectedCodePage(UINT cp);

		// Converts input to UTF-8. Always tries to read a whole number of characters.
		// To ensure at least one character can be received, nrBytesMax MUST be at least Utf8::MaxBytesPerChar.
		// Returns number of bytes read, or zero if all characters in the stream have been read.
		static sizet Read(byte* buf, sizet nrBytesMax);
		static sizet ReadLine(byte* buf, sizet nrBytesMax);

		// Reads a single Unicode character. Returns UINT_MAX if all bytes in the stream have been read.
		static uint ReadChar(Rp<StopCtl> const& stopCtl);
		static uint ReadChar() { return ReadChar(nullptr); }

		// Thread-safe in the sense that output of concurrent calls will be serialized
		static void Write(Device device, Seq text);
		static inline void Out(Seq text) { Write(StdOut, text); }
		static inline void Err(Seq text) { Write(StdErr, text); }

		static void Flush(Device device);
		static inline void FlushOut() { Flush(StdOut); }
		static inline void FlushErr() { Flush(StdErr); }
	};

}
