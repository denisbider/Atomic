#pragma once

#include "AtAbortable.h"


namespace At
{

	class Reader : virtual public IAbortable
	{
	public:
		struct Err : CommunicationErr { Err(Seq msg) : CommunicationErr(msg) {} };
		struct ReachedEnd  : Err { ReachedEnd() : Err("Reached end of data") {} };
		struct CapExceeded : Err { CapExceeded() : Err("Capacity exceeded") {} };
		struct ReadWinErr : WinErr<Err> { ReadWinErr(int64 rc) : WinErr<Err>(rc, "Error reading data") {} };

		// The internal buffer must be pinned if e.g. the Seq result of ReadVarStr is going to be kept around for any length of time without copying.
		void PinBuffer(sizet capacity) { m_buf.ReserveExact(capacity); m_bufferPinned = true; }
		void UnpinBuffer() { m_bufferPinned = false; }

		// Any buffer already in 'dest' becomes the internal Reader buffer, and the internal Reader buffer is swapped into 'dest'.
		// This allows extraction and storage of processed data in a previously pinned buffer without moving it, so that pointers to said data stay valid.
		void SwapOutProcessed(Str& dest);
		void DiscardProcessed() { Str dummy; SwapOutProcessed(dummy); }

		struct Instr { enum E { Done, NeedMore }; };
		virtual void Read(std::function<Instr::E(Seq&)> process) = 0;

		void ReadVarStr(Seq& s, sizet maxLen = SIZE_MAX);
		void ReadVarSInt64(int64& n);
		void ReadVarUInt64(uint64& n);

	protected:
		bool m_bufferPinned         {};
		Str m_buf;
		sizet m_bytesInBuf          {};
		sizet m_bytesInBufProcessed {};
		bool m_haveNewDataToProcess {};

		bool ProcessData(std::function<Instr::E(Seq&)> process);
		sizet PrepareToRead(sizet desiredReadSize, sizet minReadSize);
	};

}
