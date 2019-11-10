#pragma once

#include "AtEvent.h"
#include "AtMap.h"
#include "AtMutex.h"
#include "AtSeq.h"
#include "AtStopCtl.h"
#include "AtTime.h"

namespace At
{

	class TxScheduler
	{
	public:
		static uint64 const InvalidStxId = UINT64_MAX;

		// Enters into a queue to start a transaction with the specified type. Types are used to identify transactions that are likely to conflict.
		//
		// The number of transaction types should be finite, and should NOT depend on unique information such as database IDs.
		// If the number of possible transaction types is unbounded, this will cause runaway storage costs for statistics.
		// The current implementation uses std::type_info, the number of which is finite in a running program by definition.
		//
		// This method will block and wait if another transaction is currently queued or in progress such that, according to statistics,
		// conflict probability would exceed a threshold if the transactions were to run concurrently.
		//
		// When the transaction can run, returns a scheduled TX ID (stxId) which identifies the transaction. If the transaction retries,
		// this must be reported to TxScheduler by passing the same stxId to TxS_Conflict. When the transaction terminates without retrying,
		// or if it is canceled for external reasons, the stxId must be passed to TxS_NoConflict.

		uint64 TxS_Start(Rp<StopCtl> const& stopCtl, TypeIndex txType);

		struct ConcurrentTx
		{
			uint64    m_stxId {};
			TypeIndex m_txType {};
			Time      m_timeStarted;
			Time      m_timeEnded;
		};

		// Attempts to retrieve information about the specified transaction. Should be called at the location where transaction conflict
		// is detected in order to obtain information about the other transaction while that transaction is still active.
		ConcurrentTx TxS_GetConcurrentTx(uint64 stxId);

		// Takes note of a transaction retry. The caller must provide the scheduled TX ID of the retried transaction, and ConcurrentTx
		// for the transaction with which this one is in conflict. This causes internal statistics to be updated to indicate an occurrence of
		// conflict between the type of one transaction, and the type of the other.
		//
		// After calling this method, the stxId of the retried transaction should not be reused. Before retrying the transaction,
		// a new stxId needs to be obtained using TxS_Start.

		void TxS_Conflict(uint64 stxId, ConcurrentTx const& conflictTx);

		// Takes note of a transaction's termination without retry. Termination can be successful or unsuccessful. If unsuccessful,
		// TxS_NoConflict should be used for those transaction aborts that are not retries due to transaction conflict.

		void TxS_NoConflict(uint64 stxId);

	private:
		// ScheduledTx entries need to be kept around for as long as they might be referenced. To ensure this, when a transaction is reported ended,
		// we account for all transactions that have started and not yet ended, and keep the currently ended ScheduledTx entry around until all of
		// the other transactions that are still running and could reference this transaction have ended.

		struct ScheduledTx
		{
			uint64            m_stxId;
			TypeIndex         m_txType;
			Vec<uint64>       m_conflictStxIds;
			HANDLE            m_hTriggerEvent { INVALID_HANDLE_VALUE };
			Time              m_timeStarted;
			Time              m_timeEnded;
			Vec<ConcurrentTx> m_txsRunningAtStart;
			Vec<uint64>       m_holdsEndedStxIds;
			sizet             m_nrHeldBy {};

			ScheduledTx(uint64 stxId, TypeIndex txType) : m_stxId(stxId), m_txType(txType) {}
			uint64 Key() const { return m_stxId; }
		};

		using ScheduledTxs = Map<ScheduledTx>;

		// Statistics are kept track of as follows:
		// - The first-order entry, TxType, lists the types of transactions that were started.
		// - The second-order entry, OtherTxType, lists the number of successes and conflicts for each transaction type combination.
		//
		// Note that TxType contains the type of the transaction started later, and OtherTxType references transactions started earlier.
		// To verify if there is a conflict, look up the type of the transaction meant to be started in TxTypes, and then look for conflicts
		// with already currently running transactions under OtherTxTypes in the candidate transaction's TxType.

		struct ObservationWindow
		{
			uint64 m_nrSuccesses {};
			uint64 m_nrConflicts {};
			Time   m_windowStartTime;

			enum { WindowMinutes = 1 };
			bool Any() const { return m_windowStartTime.Any(); }
			bool IsActive(Time now) const { return m_windowStartTime.Any() && (now - m_windowStartTime) < Time::FromMinutes(ObservationWindow::WindowMinutes); }
			ObservationWindow& ResetAndStart(Time now) { *this = ObservationWindow(); m_windowStartTime = now; return *this; }
			bool CheckReset(Time now);
		};

		struct OtherTxType
		{
			TypeIndex         m_otherTxType;
			ObservationWindow m_window1;
			ObservationWindow m_window2;
			bool              m_haveConflict {};

			OtherTxType(TypeIndex otherTxType) : m_otherTxType(otherTxType) {}
			TypeIndex Key() const { return m_otherTxType; }
			bool CheckReset(Time now);
			ObservationWindow& SelectCurrentWindow(Time now);
			void Reanalyze();
		};

		using OtherTxTypes = Map<OtherTxType>;

		struct TxType
		{
			TypeIndex       m_txType;
			OtherTxTypes    m_otherTxTypes;
			Time            m_timeLastSeen;

			TxType(TypeIndex txType) : m_txType(txType) {}
			TypeIndex Key() const { return m_txType; }
		};

		using TxTypes = Map<TxType>;

		Mutex        m_mx;
		uint64       m_nextStxId {};
		ScheduledTxs m_scheduledTxs;
		TxTypes      m_txTypes;
		Vec<Event>   m_eventStorage;

		bool DoesScheduledTxConflict(Time now, ScheduledTxs::It const& stxIt, ScheduledTxs::It const& otherStxIt);
		void EnumerateScheduledTxConflicts(Time now, ScheduledTxs::It const& stxIt);

		enum ObservationType { NoConflict, Conflict };
		void AddObservation(Time now, TypeIndex txType, TypeIndex otherTxType, ObservationType observationType);

		void StartScheduledTx(Time now, ScheduledTx& stx);
		void TriggerWaitingScheduledTxs(uint64 doneStxId);
		void ReleaseHeldScheduledTxs(uint64 doneStxId, ScheduledTxs::It& doneStxIt);
		void OnScheduledTxEnd(uint64 doneStxId, ScheduledTxs::It doneStxIt);
	};

}
