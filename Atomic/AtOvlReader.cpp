#include "AtIncludes.h"
#include "AtOvlReader.h"

#include "AtAuto.h"
#include "AtNumCvt.h"


namespace At
{

	void OvlReader::Read(std::function<Instr::E(Seq&)> process)
	{
		while (true)
		{
			if (ProcessData(process))
				return;

			// Prepare to read
			DWORD readSize { NumCast<DWORD>(PrepareToRead(m_readSize, 1)) };
			ZeroMemory(&m_ovl, sizeof(m_ovl));
			m_ovl.hEvent = m_readEvent.Handle();

			// Read
			int64       errorCode   {};
			DWORD       nrBytesRead {};
			IoResult::E ioResult    { OvlReader_Read(m_buf.Ptr() + m_bytesInBuf, readSize, nrBytesRead, m_ovl, errorCode) };
			if (ioResult == IoResult::Pending)
			{
				OnExit autoCancel([&] () { OvlReader_Cancel(m_ovl); });

				// Wait for result
				AbortableWait(m_readEvent.Handle());

				// Obtain result
				ioResult = OvlReader_GetOvlResult(m_ovl, nrBytesRead, true, errorCode);
				if (ioResult == IoResult::Pending)
					throw ReadWinErr(ERROR_IO_PENDING);

				autoCancel.Dismiss();
			}

			// Check result
			if (ioResult == IoResult::Error)      throw ReadWinErr(errorCode);
			if (ioResult == IoResult::ReachedEnd) throw ReachedEnd();
			EnsureAbort(ioResult == IoResult::Done);

			if (nrBytesRead != 0)
			{
				EnsureAbort(nrBytesRead <= readSize);
				m_bytesInBuf += nrBytesRead;
				m_haveNewDataToProcess = true;
			}
		}
	}

}
