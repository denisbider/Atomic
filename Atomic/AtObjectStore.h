#pragma once

#include "AtAuto.h"
#include "AtCache.h"
#include "AtEncode.h"
#include "AtEvent.h"
#include "AtException.h"
#include "AtFile.h"
#include "AtHashMap.h"
#include "AtMap.h"
#include "AtMutex.h"
#include "AtPageAllocator.h"
#include "AtRpVec.h"
#include "AtStopCtl.h"
#include "AtStr.h"
#include "AtTime.h"
#include "AtTxScheduler.h"


// Backing up an ObjectStore database:
//
// * While not in use:
//
//   Simply copy all files in the database directory and subdirectories.
//
// * While in use:
//
//   Use Volume Shadow Copy to create a copy of the database directory and subdirectories at a point in time.
//   The ObjectStore interacts with the filesystem in such a way that any copy performed at a point in time
//   will be resumable to a consistent transaction state, as long as Journal.dat is included in the copy.
//
//   Example minimal script for DiskShadow utility (not tested):
//
//     set context persistent nowriters
//     add volume d: alias Shadow
//     expose %Shadow% s:
//     exec c:\path\to\DbBackup.cmd
//     delete shadows exposed s:
//
//   There are other ways of creating a temporary shadow copy, e.g. using a PowerShell script.


namespace At
{
	enum { Storage_PageSize = 4096 };

	class StorageFile : public File
	{
	public:
		enum class CachedBy { Unknown, Store, OS };

		void         SetId          (byte id)      { m_id = id; }
		byte         Id             () const       { return m_id; }

		void         AddOldFullPath (Seq fullPath) { m_oldFullPaths.Add(fullPath); }
		void         SetFullPath    (Seq fullPath) { m_fullPath = fullPath; }
		Seq          FullPath       () const       { return m_fullPath; }

		virtual void Open           () = 0;

		bool         IsStoreCached  () const       { return m_cachedBy == CachedBy::Store; }
		uint64       FileSize       () const       { return m_fileSize; }

		void         ReadPages          (void* firstPage, sizet nrPages, uint64 offset);
		void         ReadBytesUnaligned (void* pDestination, sizet bytesToRead, uint64 offset);
		void         WritePages         (void const* firstPage, sizet nrPages, uint64 offset);
		void         SetEof             (uint64 offset);

	protected:
		byte     m_id			{ 255 };			// ObjectStore::FileId::None
		Vec<Str> m_oldFullPaths;
		Str      m_fullPath;
		CachedBy m_cachedBy {};
		uint64   m_fileSize	{};
		uint64   m_lastOffset {};

		void OpenInner(CachedBy cachedBy);
		void CheckOldPathsAndRename();
		void ReadInner(void* pDestination, DWORD bytesToRead, uint64 offset);
	};

	class StorageFile_OsCached : public StorageFile
		{ public: virtual void Open() override final { StorageFile::OpenInner(CachedBy::OS); } };

	class StorageFile_StoreCached : public StorageFile
		{ public: virtual void Open() override final { StorageFile::OpenInner(CachedBy::Store); } };



	class EntryFile : public StorageFile
	{
	public:
		enum { HeaderBytes = 4096 };

		// Minimum size *including* any stored length
		void  SetObjSizeMin (sizet objSizeMin) { m_objSizeMin = objSizeMin; }
		sizet ObjSizeMin    () const           { return m_objSizeMin; }

		// Maximum size *including* any stored length
		void  SetObjSizeMax (sizet objSizeMax) { m_objSizeMax = objSizeMax; }
		sizet ObjSizeMax    () const           { return m_objSizeMax; }

		bool const OneOrMoreEntriesPerPage () const { return m_objSizeMax <= Storage_PageSize; };
		bool const MultiplePagesPerEntry   () const { return m_objSizeMin >  Storage_PageSize; };

		virtual void Open() override final;

	protected:
		sizet m_objSizeMin {};
		sizet m_objSizeMax { SIZE_MAX };
	};



	class FreeFile  : public EntryFile {};
	class DataFile  : public EntryFile {};
	class MetaFile  : public StorageFile_StoreCached {};
	class IndexFile : public EntryFile {};



	class JournalFile : public StorageFile_StoreCached
	{
	public:
		~JournalFile();

		void SetPageAllocator(PageAllocator& pageAllocator) { m_pageAllocator = &pageAllocator; }
		void Clear();

	private:
		PageAllocator* m_pageAllocator {};
		void*          m_clearPage     {};
	};



	struct ObjId
	{
		ObjId() = default;
		ObjId(uint64 uniqueId, uint64 index) noexcept : m_uniqueId(uniqueId), m_index(index) {}

		static ObjId None;
		static ObjId Root;

		static ObjId Next(ObjId x) noexcept { ObjId k(x); ++k.m_index; return k; }

		bool Any() const noexcept { return *this != ObjId::None; }

		bool operator<  (ObjId x) const noexcept { return m_uniqueId < x.m_uniqueId || (m_uniqueId == x.m_uniqueId && m_index < x.m_index); }
		bool operator== (ObjId x) const noexcept { return m_uniqueId == x.m_uniqueId && m_index == x.m_index; }
		bool operator<= (ObjId x) const noexcept { return operator<(x) || operator==(x); }
		bool operator!= (ObjId x) const noexcept { return !operator==(x); }

		enum { EncodedSize = 16 };

		void        EncodeBin     (Enc& enc) const;
		bool        DecodeBin     (Seq& s);
		static bool SkipDecodeBin (Seq& s);
	
		void        EncObj     (Enc& s) const;
		bool        ReadStr    (Seq& s);

		uint64 m_uniqueId {};
		uint64 m_index    {};
	};



	struct ObjIdWithRef
	{
		ObjIdWithRef() {}
		ObjIdWithRef(ObjId objId, ObjId refObjId) : m_objId(objId), m_refObjId(refObjId) {}

		ObjId m_objId;
		ObjId m_refObjId;
	};



	enum class Exclusive { No, Yes };

	class Storage : public NoCopy
	{
	public:
		struct RetryTxException : public StrErr
		{
			TxScheduler::ConcurrentTx m_conflictTx;

			RetryTxException(TxScheduler& txScheduler, uint64 conflictStxId, Seq msg) : StrErr(msg)
			{
				EnsureThrow(conflictStxId != TxScheduler::InvalidStxId);
				m_conflictTx = txScheduler.TxS_GetConcurrentTx(conflictStxId);
			}
		};
		
		struct Tainted : public ZLitErr { Tainted() : ZLitErr("The database must be re-initialized") {} };

		struct TryRunTxParams
		{
			uint  m_maxAttempts        {    4 };
			DWORD m_minWaitMs          {    0 };
			DWORD m_waitMultiplyFactor {   10 };
			DWORD m_maxWaitMs          { 1000 };
		};

	public:
		struct Stats
		{
			enum Action { Keep, Clear };
			enum { MaxRetriesTracked = 10 };

			sizet m_nrRunTxExclusive       {};
			sizet m_nrTryRunTxNonExclusive {};
			sizet m_nrNonExclusiveGiveUps  {};
			sizet m_nrStartTx              {};
			sizet m_nrCommitTx             {};
			sizet m_nrAbortTx              {};

			sizet m_nrNonExclusiveRetries[MaxRetriesTracked] {};	// Value at [N] = number of retries with N previous attempts; [0] = first attempt retries
		};

	public:
		Storage(Storage* defImpl = nullptr) : m_defImpl(defImpl) {}

		// Must be called before Init(), and cannot be called after.
		virtual void  SetDirectory               (Seq path);

		// May be called before Init(), and cannot be called after.
		virtual void  SetOpenOversizeFilesTarget (sizet target);
		virtual void  SetCachedPagesTarget       (sizet target);
		virtual void  SetVerifyCommits           (bool verifyCommits);
		virtual void  SetWritePlanTest           (bool enable, uint32 writeFailOdds);

		// Initializes a database in the directory configured using SetDirectory().
		virtual void  Init                       ();

		// May be called only after Init().
		virtual bool  HaveTx                     ();
		virtual void  RunTx                      (Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc);
		virtual void  RunTxExclusive             (std::function<void()> txFunc);
		virtual bool  TryRunTxNonExclusive       (Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc, TryRunTxParams params = TryRunTxParams());

		virtual Stats GetStats(Stats::Action action);

		// May be called only during a transaction.
		virtual void  AddPostAbortAction         (std::function<void()> action);
		virtual void  AddPostCommitAction        (std::function<void()> action);

	private:
		Storage* m_defImpl;
	};



	class ObjectStore : public Storage
	{
	// Initialization
	public:
		enum { DefaultOpenOversizeFilesTarget = 1000, DefaultCachedPagesTarget = 250000 };

		~ObjectStore() noexcept;

		void SetDirectory               (Seq path                          ) override final;
		void SetOpenOversizeFilesTarget (sizet target                      ) override final;
		void SetCachedPagesTarget       (sizet target                      ) override final;
		void SetVerifyCommits           (bool verifyCommits                ) override final;
		void SetWritePlanTest           (bool enable, uint32 writeFailOdds ) override final;
		void Init                       (                                  ) override final;

	// Transactions
	public:
		enum { MaxSingleReadObjectSize = Storage_PageSize - 2, MaxDoubleReadObjectSize = (2 * Storage_PageSize) - 2 };
		enum { NrDataFiles = 16, IndexEntryBytes = 16, IndexPagesPerFreeFilePage = 2, UInt64PerPage = Storage_PageSize / 8 };
		enum { DefaultOsCachedDataFileIncrementBytes = 1024*1024 };
		struct FileId { enum { Meta = 1, Index = 2, IndexFree = 3, DataStart = 10, DataFreeStart = 40, MaxNonSpecial = DataFreeStart + NrDataFiles - 1,
							   SpecialBit = 128, Oversize = 253, Journal = 254, None = 255 }; };

		struct ObjectState  { enum E { Unknown, ExistsButNotLoaded, Loaded, Removed, ToBeInserted }; };
		struct ObjectAction { enum E { None, Insert, Replace, Remove }; };

		struct TouchedObject : NoCopy
		{
			TouchedObject(ObjId objId) : m_objId(objId) {}
		
			void ClearUncommittedAction()
			{
				m_uncommittedAction = ObjectAction::None;
				m_uncommittedData.Clear();
				m_uncommittedTxNr = 0;
				m_uncommittedStxId = TxScheduler::InvalidStxId;
			}

			ObjId           m_objId;
			sizet           m_refCount            {};

			ObjectState::E  m_committedState      { ObjectState::Unknown };
			Rp<RcStr>       m_committedData;
			uint64          m_commitNr            {};
			uint64          m_commitStxId         {};
		
			ObjectAction::E m_uncommittedAction   { ObjectAction::None };
			Rp<RcStr>       m_uncommittedData;
			uint64          m_uncommittedTxNr     {};
			uint64          m_uncommittedStxId    {};
		};

		struct RetrievedObject
		{
			RetrievedObject(TouchedObject* tob) : m_objId(tob->m_objId), m_touchedObject(tob), m_retrievedCommitNr(tob->m_commitNr) {}

			ObjId Key() const { return m_objId; }

			bool Changed() const
			{
				EnsureAbort(m_touchedObject->m_commitNr >= m_retrievedCommitNr);
				return      m_touchedObject->m_commitNr >  m_retrievedCommitNr;
			}

			ObjId          m_objId;
			TouchedObject* m_touchedObject;
			uint64         m_retrievedCommitNr;
		};

		struct Tx : public RefCountable, NoCopy
		{
			enum State { InProgress, Aborted, Committed };

			uint64                             m_stxId;					// TxScheduler's transaction ID. For exclusive transactions, this is set to TxScheduler::InvalidStxId
			State                              m_state;
			sizet                              m_transactionsIndex;
			uint64                             m_txNr;
			Map<RetrievedObject>               m_retrievedObjects;
			Vec<TouchedObject*>                m_objectsToInsert;
			Vec<TouchedObject*>                m_objectsToReplace;
			Vec<TouchedObject*>                m_objectsToRemove;
			Vec<TouchedObject*>                m_objectsToKeepAround;
			std::vector<std::function<void()>> m_postAbortActions;		// Can't use Vec - std::function<void()> is not nothrow move constructible
			std::vector<std::function<void()>> m_postCommitActions;		// Can't use Vec - std::function<void()> is not nothrow move constructible

			void AddRetrievedObject(TouchedObject* tob) { m_retrievedObjects.Add(RetrievedObject(tob)); }
			RetrievedObject const* FindRetrievedObject(ObjId objId) const { return m_retrievedObjects.Find(objId).Ptr(); }
		};

		// Checks if the thread is in a transaction
		bool HaveTx() override final { return TlsGetValue(m_tlsIndex) != nullptr; }

		// Tries to run transaction as non-exclusive. Falls back to exclusive if transaction doesn't complete within a set number of attempts
		void RunTx(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc) override final;

		// Runs a transaction exclusively and commits it. If the transaction ends with any exception, including RetryTxException,
		// the transaction is aborted and the exception is re-thrown.
		void RunTxExclusive(std::function<void()> txFunc) override final;

		// Runs a transaction and commits it. Retries transaction a set number of times if it ends with RetryTxException.
		// Aborts transaction if it ends with any other exception. Returns true if transaction was completed and committed.
		// Returns false if transaction ended with RetryTxException each attempt. Re-throws exception if transaction aborted.
		// Throws ExecutionAborted if transaction must retry, and the stop event is set. Does not check stop event if no retry needed.
		bool TryRunTxNonExclusive(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc, TryRunTxParams params = TryRunTxParams()) override final;

		// Obtains a copy of statistics in a thread-safe manner. Can clear statistics if specified.
		Stats GetStats(Stats::Action action) override final;

		// Adds an action which will be executed if the current transaction aborts.
		// The action must be implemented carefully: if it throws, the program will abort.
		void AddPostAbortAction(std::function<void()> action) override final;

		// Adds an action which will be executed if the current transaction successfully commits.
		// The action must be implemented carefully: if it throws, the program will abort.
		void AddPostCommitAction(std::function<void()> action) override final;

	// Usage
	public:
		// ObjectStore implements serializable transaction semantics.
		// If an action cannot proceed because allowing it to proceed would cause transactions to no longer be serializable,
		// RetryTxException is thrown, and the transaction is expected to be retried.

		// Inserts an object into storage, and returns an ID that can be used with RetrieveObject() to retrieve it.
		ObjId InsertObject(Rp<RcStr> const& data);

		// Retrieves an existing object from the database, given an ID previously returned from InsertObject().
		// The second parameter, refObjId, optionally specifies the object from which retrieveObjId was read. If provided, the reference
		// object is first checked to verify that it exists, and has not been updated since the current transaction read from it.
		// Returns a null pointer if the object with the specified ID does not exist, or has been removed, or if ObjId::None is passed.
		Rp<RcStr> RetrieveObject(ObjId retrieveObjId, ObjId refObjId);

		// Replaces an existing object, keeping the same object ID.
		// The second parameter, refObjId, optionally specifies the object from which retrieveObjId was read. If provided, the reference
		// object is first checked to verify that it exists, and has not been updated since the current transaction read from it.
		void ReplaceObject(ObjId objId, ObjId refObjId, Rp<RcStr> data);

		// Removes an object from the database.
		// The second parameter, refObjId, optionally specifies the object from which retrieveObjId was read. If provided, the reference
		// object is first checked to verify that it exists, and has not been updated since the current transaction read from it.
		void RemoveObject(ObjId objId, ObjId refObjId);

	// Private types and data
	private:
		PageAllocator m_pageAllocator;
		DWORD m_tlsIndex { TLS_OUT_OF_INDEXES };

		// Initialization data
		bool   m_inited              {};
		bool   m_tainted             {};
		Str    m_dir;
		bool   m_verifyCommits       {};
		bool   m_writePlanTest       {};
		uint32 m_writeFailOdds       {};
		bool   m_completingWritePlan {};

		// Files
		MetaFile    m_metaFile;
		IndexFile   m_indexFile;
		FreeFile    m_indexFreeFile;						// Entries are indices, not offsets into index file
		DataFile    m_dataFiles[NrDataFiles];
		FreeFile    m_dataFreeFiles[NrDataFiles];			// Entries are offsets into data file
		JournalFile m_journalFile;

		struct StructuredOffset
		{
			uint64 pageOffsetInFile;
			uint64 offsetInPage;
		};

		// Some fields are updated with normal increments when m_mx is locked.
		// Other fields are incremented atomically with no other locking.
		Stats volatile m_stats;

		// Transactions
		TxScheduler m_txScheduler;
		Mutex       m_mxStart;
		Mutex       m_mx;
		uint64      m_lastUniqueId          {};
		uint64      m_lastWrittenUniqueId   {};
		Str         m_lastWriteStateHash;
		uint64      m_lastTxNr              {};
		uint64      m_lastCommitNr          {};
		RpVec<Tx>   m_transactions;
		sizet       m_nrActiveTransactions  {};
		bool        m_needNoTxNotification  {};
		Event       m_noTxNotificationEvent { Event::CreateAuto };

		struct FreeIndexState { enum E { Invalid, Available, Reserved }; };

		struct FisPage
		{
			FisPage() { m_state.ResizeExact(UInt64PerPage); }
			FisPage(FisPage const&) = delete;

			FisPage(FisPage&& x) noexcept
			{
				NoExcept(m_state.Swap(x.m_state));
				NoExcept(std::swap(m_nrInvalid, x.m_nrInvalid));
				NoExcept(std::swap(m_nrAvailable, x.m_nrAvailable));
				NoExcept(std::swap(m_nrReserved, x.m_nrReserved));
			}

			Str   m_state;								// Each value indicates the state of the corresponding entry on the m_indexFreeFile page.
			sizet m_nrInvalid   {};
			sizet m_nrAvailable {};
			sizet m_nrReserved  {};
		};

		Vec<FisPage> m_fisPages;						// First/second/... page contains the state of first/second/... page in m_indexFreeFile.
		uint64       m_fisPages_totalNrInvalid {};		// Total number of invalid entries in m_fisPages

		struct TouchedObjectEntry : NoCopy
		{
			TouchedObjectEntry(TouchedObject* tob) : m_touchedObject(tob) {}
			TouchedObjectEntry(TouchedObjectEntry&& x) noexcept : m_touchedObject(x.m_touchedObject) { x.m_touchedObject = nullptr; }
			~TouchedObjectEntry() noexcept { delete m_touchedObject; }

			static uint64 HashOfKey(ObjId k) { return k.m_index; }
			ObjId Key() const { return m_touchedObject->m_objId; }

			TouchedObject* m_touchedObject {};
		};

		HashMap<TouchedObjectEntry> m_touchedObjects;

		// Cached pages
		struct CachedPage
		{
			PageAllocator* m_pageAllocator {};
			byte*          m_page          {};

			~CachedPage() { if (m_page != nullptr) m_pageAllocator->ReleasePage(m_page); }

			bool Inited() const { return m_page != nullptr; }
			void Init(PageAllocator& pageAllocator) { EnsureAbort(m_page == nullptr); m_pageAllocator = &pageAllocator; m_page = m_pageAllocator->GetPage(); }
		};

		struct Locator
		{
			enum EOversize { Oversize };

			byte m_fileId { FileId::None };
			ObjId m_oversizeFileId;
			uint64 m_offset {};

			Locator() {}
			Locator(StorageFile* sf, uint64 offset) { Set(sf, offset); }
			Locator(EOversize, ObjId oversizeFileId) { Set(Oversize, oversizeFileId); }

			void Set(StorageFile* sf, uint64 offset);
			void Set(EOversize, ObjId oversizeFileId);
		
			bool operator< (Locator const& x) const;
			bool operator== (Locator const& x) const;
			bool operator!= (Locator const& x) const { return !operator==(x); }
		};

		sizet                              m_openOversizeFilesTarget { DefaultOpenOversizeFilesTarget };
		sizet                              m_cachedPagesTarget       { DefaultCachedPagesTarget };
		Cache<ObjId, StorageFile_OsCached> m_openOversizeFiles;
		Cache<Locator, CachedPage>         m_cachedPages;

		// Write log
		struct WritePlanEntry
		{
			enum { SerializedBytes_Base                 = 1 + 1 + 16 + 8,
				   SerializedBytes_DeleteOversizedFile  = SerializedBytes_Base,
				   SerializedBytes_WriteSinglePage      = SerializedBytes_Base + Storage_PageSize,
				   SerializedBytes_WriteMultiPageHeader = SerializedBytes_Base + 4 };

			enum { EntryTypeMask = 0x7F, Flag_MultiPage = 0x80 };

			enum Type { Undefined = 0, Write = 1, WriteEof = 2, DeleteOversizeFile = 3 };

			WritePlanEntry() {}

			// This version of constructor used for type == DeleteOversizeFile.
			WritePlanEntry(Locator const& locator, Type type)
				: m_locator(locator), m_type(type), m_firstPage(nullptr), m_nrPages(0), m_entryIndex(SIZE_MAX) {}

			// This version of constructor used for single-page writes of cached pages. The page must be already in cache.
			// Pointer to first page is non-const because writes to beginning of meta file get modified in PerformWritePlanActions().
			WritePlanEntry(Locator const& locator, void* firstPage, Type type = Write)
				: m_locator(locator), m_type(type), m_firstPage((byte*) firstPage), m_nrPages(1), m_entryIndex(SIZE_MAX) {}

			// This version of constructor used for multi-page writes. Accepts reference-counted sequential pages not in cache.
			WritePlanEntry(Locator const& locator, Rp<Rc<SequentialPages>> const& pages, Type type = Write)
				: m_locator(locator), m_type(type), m_firstPage(pages->Ptr()), m_nrPages(pages->Nr()), m_pages(pages), m_entryIndex(SIZE_MAX) {}

			Locator m_locator;
			Type    m_type       { Undefined };
			byte*   m_firstPage  {};
			sizet   m_nrPages    {};
			sizet   m_entryIndex { SIZE_MAX };

			Rp<Rc<SequentialPages>> m_pages;
		};

		typedef Vec<WritePlanEntry*> WritePlanEntries;
		typedef std::map<Locator, WritePlanEntry*> WritePlanEntriesByLocator;
	
		struct WritePlanState { enum E { None, Started, Executing }; };

		class WritePlanEncoder
		{
		public:
			WritePlanEncoder(byte* pWrite, sizet nrBytesRemaining) : m_pWrite(pWrite), m_nrBytesRemaining(nrBytesRemaining) {}
			void Encode(Seq s) { Encode(s.p, s.n); }
			void Encode(void const* p, sizet n);
			sizet Remaining() const { return m_nrBytesRemaining; }

		private:
			byte* m_pWrite {};
			sizet m_nrBytesRemaining {};
		};

		WritePlanState::E         m_writePlanState { WritePlanState::None };
		WritePlanEntries          m_writePlanEntries;
		WritePlanEntriesByLocator m_replaceableWritePlanEntries;
		Vec<uint64>               m_writePlanFileSizes;						// Indexed by file ID. There's unused space, but that's OK.

	// Private functions
	private:

		void             StartTx                               (uint64 stxId);
		void             AbortTx                               ();
		void             CommitTx                              ();

		void             EndTxCommon                           (Tx* tx);
		void             AbortTx_Inner                         (Tx* tx);
		void             CommitTx_MakeWritePlan                (Tx* tx, uint64 commitNr);
		sizet            CommitTx_InsertObjects                (Tx* tx, uint64 commitNr);
		void             CommitTx_ReplaceObjects               (Tx* tx, uint64 commitNr);
		sizet            CommitTx_RemoveObjects                (Tx* tx, uint64 commitNr);
		void             CommitTx_InsertObjectData             (Seq data, DataFile* df, FreeFile* ff, uint64& offset);
		void             CommitTx_WriteObjectData              (Seq data, DataFile* df, uint64 offset);
		uint64           CommitTx_ClaimDataFileFreeEntryOffset (DataFile* df, FreeFile* ff);
		void             CommitTx_RemoveObjectData             (byte fileId, uint64 offset);
		void             CommitTx_RemoveObjectData             (DataFile* df, FreeFile* ff, uint64 offset);
		void             CommitTx_VerifyObject                 (TouchedObject* to);

		void             InsertTouchedObject                   (AutoFree<TouchedObject>& tob);
		TouchedObject*   FindTouchedObjectInMemory             (ObjId objId);
		void             LoadTouchedObjectExistence            (TouchedObject* tob);
		void             LoadTouchedObjectFromDisk             (TouchedObject* tob);
		void             DecrementTouchedObjectRefCount        (TouchedObject* tob);
		void             AddTouchedObjectToKeepAround          (Tx* tx, TouchedObject* tob);

		StructuredOffset GetStructuredOffset                   (uint64 offsetInFile);

		void             LoadMetaData                          ();
		void             LoadFisPages                          ();
		uint64           ReserveIndex                          ();
		void             ConsolidateFreeIndexState             ();

		// Returns false if object is oversize (too large for storage files)
		bool             FindDataFileByObjectSizeNoLen         (sizet n, DataFile*& df, FreeFile*& ff);

		uint64           GetWritePlanFileSize                  (StorageFile* sf);

		StorageFile*     GetOversizeFile                       (ObjId oversizeFileId);
		void             DeleteOversizeFile                    (ObjId oversizeFileId);
		Str              GetOversizeDirPath                    ();
		Str              GetOversizeFilePath                   (ObjId oversizeFileId);

		byte*            CachedReadPage                        (StorageFile* sf, uint64 offset);

		void             PruneCache                            ();

		StorageFile*     FindStorageFileById                   (byte fileId);
		DataFile*        FindDataFileById                      (byte fileId);
		FreeFile*        FindDataFreeFileById                  (byte fileId);

		void             RunWritePlan                          (std::function<void()> f);
		void             StartWritePlan                        ();
		void             ClearWritePlan                        ();
		void             AddWritePlanEntry                     (AutoFree<WritePlanEntry>& entry);
		void             RecordAndExecuteWritePlan             ();
		void             PerformWritePlanActions               ();
		bool             CompleteWritePlanFromJournal          ();

		uint64 MakeCompactLocator      (byte fileId, uint64 offset ) const { EnsureAbort((offset >> 56) == 0); return (((uint64) fileId) << 56) | offset; }
		byte   GetCompactLocatorFileId (uint64 compactLocator      ) const { return (compactLocator >> 56) & 0xFF; }
		uint64 GetCompactLocatorOffset (uint64 compactLocator      ) const { return (compactLocator & 0xFFFFFFFFFFFFFF); }

		void AddWritePlanEntry_DeleteOversizeFile(Locator const& locator)
			{ AutoFree<WritePlanEntry> wpe(new WritePlanEntry(locator, WritePlanEntry::DeleteOversizeFile)); AddWritePlanEntry(wpe); }

		void AddWritePlanEntry_CachedPage(Locator const& locator, void* page, WritePlanEntry::Type type=WritePlanEntry::Write)
			{ AutoFree<WritePlanEntry> wpe(new WritePlanEntry(locator, page, type)); AddWritePlanEntry(wpe); }

		void AddWritePlanEntry_SequentialPages(Locator const& locator, Rp<Rc<SequentialPages>> const& pages, WritePlanEntry::Type type=WritePlanEntry::Write)
			{ AutoFree<WritePlanEntry> wpe(new WritePlanEntry(locator, pages, type)); AddWritePlanEntry(wpe); }
	};

}
