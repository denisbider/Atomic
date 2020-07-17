#include "AtIncludes.h"
#include "AtReader.h"

#include "AtEncode.h"


namespace At
{
	void Reader::SwapOutProcessed(Str& dest)
	{
		sizet bytesInBufToProcess = m_bytesInBuf - m_bytesInBufProcessed;
		dest.ResizeExact(bytesInBufToProcess);
		memmove(dest.Ptr(), m_buf.Ptr() + m_bytesInBufProcessed, bytesInBufToProcess);
		m_buf.ResizeExact(m_bytesInBufProcessed);
		dest.Swap(m_buf);
		m_bufferPinned = false;
	}


	void Reader::ReadVarStr(Seq& s, sizet maxLen)
	{
		Read(	[&] (Seq& reader) -> Reader::Instr::E
				{
					if (!CanTryDecodeVarStr(reader, maxLen)) return Reader::Instr::NeedMore;
					if (!DecodeVarStr(reader, s, maxLen)) throw InputErr("ReadVarStr: error decoding string");
					return Reader::Instr::Done;
				} );
	}


	void Reader::ReadVarSInt64(int64& n)
	{
		Read(	[&] (Seq& reader) -> Reader::Instr::E
				{
					if (!CanTryDecodeVarSInt64(reader)) return Reader::Instr::NeedMore;
					if (!DecodeVarSInt64(reader, n)) throw InputErr("ReadVarSInt64: error decoding integer");
					return Reader::Instr::Done;
				} );
	}


	void Reader::ReadVarUInt64(uint64& n)
	{
		Read(	[&] (Seq& reader) -> Reader::Instr::E
				{
					if (!CanTryDecodeVarSInt64(reader)) return Reader::Instr::NeedMore;
					if (!DecodeVarUInt64(reader, n)) throw InputErr("ReadVarUInt64: error decoding integer");
					return Reader::Instr::Done;
				} );
	}


	bool Reader::ProcessData(std::function<Instr::E(Seq&)> process)
	{
		if (m_haveNewDataToProcess)
		{
			// Process data
			Seq      reader { m_buf.Ptr() + m_bytesInBufProcessed, m_bytesInBuf - m_bytesInBufProcessed };
			Seq      orig   { reader };
			Instr::E instr  { process(reader) };

			if (instr == Instr::Done)
			{
				EnsureAbort(reader.p > orig.p);
				EnsureAbort(reader.p <= orig.p + orig.n);

				sizet bytesProcessedThisTime = (sizet) (reader.p - orig.p);
				m_bytesInBufProcessed += bytesProcessedThisTime;

				if (m_bytesInBufProcessed == m_bytesInBuf)
					m_haveNewDataToProcess = false;

				return true;
			}

			EnsureAbort(instr == Instr::NeedMore);
			m_haveNewDataToProcess = false;
		}

		return false;
	}


	sizet Reader::PrepareToRead(sizet desiredReadSize, sizet minReadSize)
	{
		sizet readSize { desiredReadSize };
		if (m_bufferPinned)
		{
			// We are maintaining a pinned buffer
			sizet maxReadSize { SatSub<sizet>(m_buf.Cap(), m_bytesInBuf) };
			if (readSize > maxReadSize)
				readSize = maxReadSize;
			if (readSize < minReadSize)
				throw CapExceeded();
		}
		else
		{
			sizet bytesInBufToProcess { m_bytesInBuf - m_bytesInBufProcessed };
			if (m_bytesInBufProcessed >= bytesInBufToProcess)
			{
				// Clean previously processed data
				memmove(m_buf.Ptr(), m_buf.Ptr() + m_bytesInBufProcessed, bytesInBufToProcess);
				m_bytesInBuf = bytesInBufToProcess;
				m_bytesInBufProcessed = 0;
			}
		}

		m_buf.ResizeExact(m_bytesInBuf + readSize);
		return readSize;
	}
}
