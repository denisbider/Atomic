#include "AtIncludes.h"
#include "AtOvlWriter.h"

#include "AtAuto.h"
#include "AtNumCvt.h"


namespace At
{

	void OvlWriter::Write(Seq data)
	{
		while (data.n != 0)
		{
			ZeroMemory(&m_ovl, sizeof(m_ovl));
			m_ovl.hEvent = m_writeEvent.Handle();

			int64 errorCode      {};
			DWORD nrBytesToWrite { (DWORD) PickMin<sizet>(m_writeSize, data.n) };
			DWORD nrBytesWritten {};
			IoResult::E ioResult { OvlWriter_Write(data.p, nrBytesToWrite, nrBytesWritten, m_ovl, errorCode) };
			if (ioResult == IoResult::Pending)
			{
				OnExit autoCancel([&] () { OvlWriter_Cancel(m_ovl); });

				// Wait for result
				AbortableWait(m_writeEvent.Handle());

				// Obtain result
				ioResult = OvlWriter_GetOvlResult(m_ovl, nrBytesWritten, true, errorCode);
				if (ioResult == IoResult::Pending)
					throw WriteWinErr(ERROR_IO_PENDING);

				autoCancel.Dismiss();
			}

			// Check result
			if (ioResult == IoResult::Error)
				throw WriteWinErr(errorCode);

			EnsureAbort(ioResult == IoResult::Done);
			EnsureAbort(nrBytesWritten <= nrBytesToWrite);
			data.DropBytes(nrBytesWritten);
		}
	}

}
