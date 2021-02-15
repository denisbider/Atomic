#include "AtIncludes.h"
#include "AtReader.h"

#include "AtEncode.h"


namespace At
{

	void Reader::SwapOutProcessed(Str& dest)
	{
		EnsureThrow(m_bytesInBuf >= m_bytesInBufProcessed);
		EnsureThrow(m_bytesInBuf <= m_buf.Len());

		sizet bytesInBufToProcess = m_bytesInBuf - m_bytesInBufProcessed;
		dest.ResizeExact(bytesInBufToProcess);
		memmove(dest.Ptr(), m_buf.Ptr() + m_bytesInBufProcessed, bytesInBufToProcess);
		m_buf.ResizeExact(m_bytesInBufProcessed);
		dest.Swap(m_buf);

		m_bufferPinned = false;
		m_bytesInBuf = bytesInBufToProcess;
		m_bytesInBufProcessed = 0;
	}


	void Reader::ReadVarStr(Seq& s, sizet maxLen)
	{
		Read(	[&] (Seq& reader) -> Reader::ReadInstr
				{
					if (!CanTryDecodeVarStr(reader, maxLen)) return Reader::ReadInstr::NeedMore;
					if (!DecodeVarStr(reader, s, maxLen)) throw InputErr("ReadVarStr: error decoding string");
					return Reader::ReadInstr::Done;
				} );
	}


	void Reader::ReadVarSInt64(int64& n)
	{
		Read(	[&] (Seq& reader) -> Reader::ReadInstr
				{
					if (!CanTryDecodeVarSInt64(reader)) return Reader::ReadInstr::NeedMore;
					if (!DecodeVarSInt64(reader, n)) throw InputErr("ReadVarSInt64: error decoding integer");
					return Reader::ReadInstr::Done;
				} );
	}


	void Reader::ReadVarUInt64(uint64& n)
	{
		Read(	[&] (Seq& reader) -> Reader::ReadInstr
				{
					if (!CanTryDecodeVarSInt64(reader)) return Reader::ReadInstr::NeedMore;
					if (!DecodeVarUInt64(reader, n)) throw InputErr("ReadVarUInt64: error decoding integer");
					return Reader::ReadInstr::Done;
				} );
	}


	Reader::KeepReading Reader::TryProcessData(std::function<ReadInstr(Seq&)> process)
	{
		if (m_haveNewDataToProcess)
		{
			// Process data
			EnsureThrow(m_bytesInBuf >= m_bytesInBufProcessed);
			EnsureThrow(m_bytesInBuf <= m_buf.Len());

			Seq       reader { m_buf.Ptr() + m_bytesInBufProcessed, m_bytesInBuf - m_bytesInBufProcessed };
			Seq       orig   { reader };
			ReadInstr instr  { process(reader) };

			if (instr == ReadInstr::Done)
			{
				EnsureAbort(reader.p > orig.p);
				EnsureAbort(reader.p <= orig.p + orig.n);

				sizet bytesProcessedThisTime = (sizet) (reader.p - orig.p);
				m_bytesInBufProcessed += bytesProcessedThisTime;
				EnsureAbort(m_bytesInBuf >= m_bytesInBufProcessed);

				if (m_bytesInBufProcessed == m_bytesInBuf)
					m_haveNewDataToProcess = false;

				return KeepReading::No;
			}

			if (instr == ReadInstr::RequiresAlternateProcessing)
			{
				EnsureThrow(reader.p == orig.p);
				EnsureThrow(reader.n == orig.n);
				return KeepReading::No;
			}

			EnsureAbort(instr == ReadInstr::NeedMore);
			m_haveNewDataToProcess = false;
		}

		return KeepReading::Yes;
	}


	Reader::ReadDest Reader::BeginRead(sizet desiredReadSize, sizet minReadSize)
	{
		EnsureThrow(m_bytesInBuf >= m_bytesInBufProcessed);
		EnsureThrow(m_bytesInBuf <= m_buf.Len());

		sizet readSize = desiredReadSize;
		if (m_bufferPinned)
		{
			// We are maintaining a pinned buffer
			sizet maxReadSize = m_buf.Cap() - m_bytesInBuf;
			if (readSize > maxReadSize)
				readSize = maxReadSize;
			if (readSize < minReadSize)
				throw CapExceeded();
		}
		else if (m_bytesInBufProcessed)
		{
			sizet bytesInBufToProcess = m_bytesInBuf - m_bytesInBufProcessed;
			if (m_bytesInBufProcessed >= bytesInBufToProcess)
			{
				// Discard previously processed data when there is more of it than data left to process
				if (bytesInBufToProcess)
					memmove(m_buf.Ptr(), m_buf.Ptr() + m_bytesInBufProcessed, bytesInBufToProcess);

				m_bytesInBuf = bytesInBufToProcess;
				m_bytesInBufProcessed = 0;
			}
		}

		EnsureThrow(readSize < (SIZE_MAX - m_bytesInBuf));
		m_buf.ResizeExact(m_bytesInBuf + readSize);

		ReadDest rd;
		rd.m_destPtr = m_buf.Ptr() + m_bytesInBuf;
		rd.m_maxBytes = readSize;
		return rd;
	}


	void Reader::CompleteRead(sizet bytesRead)
	{
		if (bytesRead)
		{
			EnsureThrow(m_bytesInBuf <= m_buf.Len());
			EnsureThrow(bytesRead <= m_buf.Len() - m_bytesInBuf);
			m_bytesInBuf += bytesRead;
			m_haveNewDataToProcess = true;
		}
	}

}
