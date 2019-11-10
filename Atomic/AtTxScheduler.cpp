#include "AtIncludes.h"
#include "AtTxScheduler.h"

#include "AtWait.h"

namespace At
{
	uint64 TxScheduler::TxS_Start(Rp<StopCtl> const& stopCtl, TypeIndex txType)
	{
		uint64 stxId;
		Event triggerEvent;

		{
			Locker locker { m_mx };
			Time now = Time::StrictNow();

			stxId = m_nextStxId++;
			ScheduledTxs::It stxIt = m_scheduledTxs.Add(stxId, txType);
			OnExit eraseStx = [&] { m_scheduledTxs.Erase(stxIt); };

			EnumerateScheduledTxConflicts(now, stxIt);

			if (!stxIt->m_conflictStxIds.Any())
			{
				StartScheduledTx(now, stxIt.Ref());
				eraseStx.Dismiss();
				return stxId;
			}

			// Acquire trigger event
			if (!m_eventStorage.Any())
				triggerEvent.Create(Event::CreateAuto);
			else
			{
				triggerEvent = std::move(m_eventStorage.Last());
				m_eventStorage.PopLast();
			}

			stxIt->m_hTriggerEvent = triggerEvent.Handle();
			eraseStx.Dismiss();
		}

		// Wait to start
		bool abortStx {};
		try
		{
			if (Wait2(stopCtl->StopEvent().Handle(), triggerEvent.Handle(), INFINITE) == 0)
				abortStx = true;
		}
		catch (std::exception const&)
		{
			Locker locker { m_mx };

			ScheduledTxs::It stxIt = m_scheduledTxs.Find(stxId);
			EnsureAbort(stxIt.Any());

			m_scheduledTxs.Erase(stxIt);
			throw;
		}

		// Start (or abort)
		{
			Locker locker { m_mx };
			Time now = Time::StrictNow();

			ScheduledTxs::It stxIt = m_scheduledTxs.Find(stxId);
			EnsureAbort(stxIt.Any());
			OnExit eraseStx = [&] { m_scheduledTxs.Erase(stxIt); };

			stxIt->m_hTriggerEvent = INVALID_HANDLE_VALUE;
			m_eventStorage.Add(std::move(triggerEvent));

			if (abortStx)
				throw ExecutionAborted();

			StartScheduledTx(now, stxIt.Ref());
			eraseStx.Dismiss();
		}

		return stxId;
	}


	TxScheduler::ConcurrentTx TxScheduler::TxS_GetConcurrentTx(uint64 stxId)
	{
		Locker locker { m_mx };

		ScheduledTxs::It stxIt = m_scheduledTxs.Find(stxId);
		EnsureAbort(stxIt.Any());
		EnsureAbort(stxIt->m_timeStarted.Any());

		ConcurrentTx ct;
		ct.m_stxId       = stxId;
		ct.m_txType      = stxIt->m_txType;
		ct.m_timeStarted = stxIt->m_timeStarted;
		ct.m_timeEnded   = stxIt->m_timeEnded;
		return ct;
	}


	void TxScheduler::TxS_Conflict(uint64 stxId, ConcurrentTx const& conflictTx)
	{
		Locker locker { m_mx };
		Time now = Time::StrictNow();

		ScheduledTxs::It stxIt = m_scheduledTxs.Find(stxId);
		EnsureAbort(stxIt.Any());
		EnsureAbort(!stxIt->m_timeEnded.Any());

		if (conflictTx.m_timeStarted < stxIt->m_timeStarted)
			AddObservation(now, stxIt->m_txType, conflictTx.m_txType, ObservationType::Conflict);
		else
			AddObservation(now, conflictTx.m_txType, stxIt->m_txType, ObservationType::Conflict);

		OnScheduledTxEnd(stxId, stxIt);
	}


	void TxScheduler::TxS_NoConflict(uint64 stxId)
	{
		Locker locker { m_mx };
		Time now = Time::StrictNow();

		ScheduledTxs::It stxIt = m_scheduledTxs.Find(stxId);
		EnsureAbort(stxIt.Any());
		EnsureAbort(!stxIt->m_timeEnded.Any());

		for (ConcurrentTx const& otherTx : stxIt->m_txsRunningAtStart)
			AddObservation(now, stxIt->m_txType, otherTx.m_txType, ObservationType::NoConflict);

		OnScheduledTxEnd(stxId, stxIt);
	}


	bool TxScheduler::ObservationWindow::CheckReset(Time now)
	{
		if (now - m_windowStartTime > Time::FromMinutes(2*WindowMinutes))
		{
			*this = ObservationWindow();
			return true;
		}

		return false;
	}


	bool TxScheduler::OtherTxType::CheckReset(Time now)
	{
		bool changed1 = m_window1.CheckReset(now);
		bool changed2 = m_window2.CheckReset(now);
		return changed1 || changed2;
	}


	TxScheduler::ObservationWindow& TxScheduler::OtherTxType::SelectCurrentWindow(Time now)
	{
		auto useNewerOrResetOlder = [now] (ObservationWindow& newer, ObservationWindow& older) -> ObservationWindow&
			{
				if (newer.IsActive(now))
					return newer;
				else
					return older.ResetAndStart(now);
			};

		if (m_window1.m_windowStartTime > m_window2.m_windowStartTime)
			return useNewerOrResetOlder(m_window1, m_window2);
		else
			return useNewerOrResetOlder(m_window2, m_window1);
	}


	void TxScheduler::OtherTxType::Reanalyze()
	{
		m_haveConflict = false;

		uint64 nrConflicts   = m_window1.m_nrConflicts + m_window2.m_nrConflicts;
		uint64 nrSuccesses   = m_window1.m_nrSuccesses + m_window2.m_nrSuccesses;
		uint64 nrTotal       = nrConflicts + nrSuccesses;

		auto conflictRatio = [=] () -> float { return ((float) nrConflicts) / ((float) nrTotal); };

			 if (nrTotal >= 64 && conflictRatio() >= 0.125 ) m_haveConflict = true;
		else if (nrTotal >= 16 && nrConflicts     >= 8     ) m_haveConflict = true;
		else if (nrTotal >=  8 && conflictRatio() >= 0.75  ) m_haveConflict = true;
	}


	bool TxScheduler::DoesScheduledTxConflict(Time now, ScheduledTxs::It const& stxIt, ScheduledTxs::It const& otherStxIt)
	{
		TxTypes::It typeIt = m_txTypes.Find(stxIt->m_txType);
		if (typeIt.Any())
		{
			OtherTxTypes::It otherTypeIt = typeIt->m_otherTxTypes.Find(otherStxIt->m_txType);
			if (otherTypeIt.Any())
			{
				if (otherTypeIt->CheckReset(now))
					otherTypeIt->Reanalyze();

				if (otherTypeIt->m_haveConflict)
					return true;
			}
		}

		return false;
	}


	void TxScheduler::EnumerateScheduledTxConflicts(Time now, ScheduledTxs::It const& stxIt)
	{
		ScheduledTxs::It otherStxIt = stxIt;
		while (true)
		{
			--otherStxIt;
			if (!otherStxIt.Valid())
				break;

			if (!otherStxIt->m_timeEnded.Any())
				if (DoesScheduledTxConflict(now, stxIt, otherStxIt))
					stxIt->m_conflictStxIds.Add(otherStxIt->m_stxId);
		}
	}


	void TxScheduler::AddObservation(Time now, TypeIndex txType, TypeIndex otherTxType, ObservationType observationType)
	{
		TxTypes::It typeIt = m_txTypes.Find(txType);
		if (!typeIt.Any())
			typeIt = m_txTypes.Add(txType);

		OtherTxTypes::It otherTypeIt = typeIt->m_otherTxTypes.Find(otherTxType);
		if (!otherTypeIt.Any())
			otherTypeIt = typeIt->m_otherTxTypes.Add(otherTxType);

		ObservationWindow& ow = otherTypeIt->SelectCurrentWindow(now);
		if (observationType == ObservationType::NoConflict)
			++ow.m_nrSuccesses;
		else
			++ow.m_nrConflicts;

		otherTypeIt->Reanalyze();
	}


	void TxScheduler::StartScheduledTx(Time now, ScheduledTx& stx)
	{
		for (ScheduledTx const& otherTx : m_scheduledTxs)
			if (otherTx.m_timeStarted.Any())
			{
				ConcurrentTx& ct = stx.m_txsRunningAtStart.Add();
				ct.m_stxId       = otherTx.m_stxId;
				ct.m_txType      = otherTx.m_txType;
				ct.m_timeStarted = otherTx.m_timeStarted;
				ct.m_timeEnded   = otherTx.m_timeEnded;
			}

		stx.m_timeStarted = now;
	}


	void TxScheduler::TriggerWaitingScheduledTxs(uint64 doneStxId)
	{
		for (ScheduledTx& candidateTx : m_scheduledTxs)
			if (!candidateTx.m_timeStarted.Any() && candidateTx.m_hTriggerEvent != INVALID_HANDLE_VALUE)
			{
				for (sizet i=0; i!=candidateTx.m_conflictStxIds.Len(); ++i)
					if (candidateTx.m_conflictStxIds[i] == doneStxId)
					{
						candidateTx.m_conflictStxIds.Erase(i, 1);
						break;
					}

				if (!candidateTx.m_conflictStxIds.Any())
				{
					Event::Signal(candidateTx.m_hTriggerEvent);
					candidateTx.m_hTriggerEvent = INVALID_HANDLE_VALUE;
				}
			}
	}


	void TxScheduler::ReleaseHeldScheduledTxs(uint64 doneStxId, ScheduledTxs::It& doneStxIt)
	{
		Vec<uint64> removeStxIds;
		for (uint64 heldStxId : doneStxIt->m_holdsEndedStxIds)
		{
			ScheduledTxs::It heldStxIt = m_scheduledTxs.Find(heldStxId);
			EnsureAbort(heldStxIt.Any());
			EnsureAbort(heldStxIt->m_timeEnded.Any());
			EnsureAbort(heldStxIt->m_nrHeldBy >= 1);
			EnsureAbort(!heldStxIt->m_holdsEndedStxIds.Any());

			if (!--heldStxIt->m_nrHeldBy)
				removeStxIds.Add(heldStxId);
		}

		doneStxIt->m_holdsEndedStxIds.Free();

		if (removeStxIds.Any())
		{
			// Each removal invalidates iterators, so lookup required after each removal
			do
			{
				ScheduledTxs::It removeStxIt = m_scheduledTxs.Find(removeStxIds.Last());
				EnsureAbort(removeStxIt.Any());
				EnsureAbort(removeStxIt->m_timeEnded.Any());
				EnsureAbort(!removeStxIt->m_nrHeldBy);
				EnsureAbort(!removeStxIt->m_holdsEndedStxIds.Any());

				m_scheduledTxs.Erase(removeStxIt);
				removeStxIds.PopLast();
			}
			while (removeStxIds.Any());

			doneStxIt = m_scheduledTxs.Find(doneStxId);
			EnsureAbort(doneStxIt.Any());
		}
	}


	void TxScheduler::OnScheduledTxEnd(uint64 doneStxId, ScheduledTxs::It doneStxIt)
	{
		doneStxIt->m_timeEnded = Time::StrictNow();

		ReleaseHeldScheduledTxs(doneStxId, doneStxIt);
		
		TriggerWaitingScheduledTxs(doneStxId);

		for (ScheduledTx& otherTx : m_scheduledTxs)
			if (otherTx.m_timeStarted.Any() && !otherTx.m_timeEnded.Any())
			{
				otherTx.m_holdsEndedStxIds.Add(doneStxId);
				++doneStxIt->m_nrHeldBy;
			}

		if (!doneStxIt->m_nrHeldBy)
			m_scheduledTxs.Erase(doneStxIt);
	}

}
