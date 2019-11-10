#include "AtIncludes.h"
#include "AtObjectStore.h"

#include "AtCrypt.h"
#include "AtNum.h"
#include "AtPath.h"
#include "AtRcHandle.h"
#include "AtWait.h"
#include "AtWinStr.h"


namespace At
{

	// StorageFile

	void StorageFile::OpenInner(CachedBy cachedBy)
	{
		EnsureAbort(m_fullPath.Any());

		if (m_oldFullPaths.Any())
			CheckOldPathsAndRename();

		DWORD flags = File::Flag::WriteThrough;
		if (cachedBy == CachedBy::Store)
			flags |= File::Flag::NoBuffering;

		File::Open(m_fullPath, OpenArgs().Access(GENERIC_READ | GENERIC_WRITE)
										 .Share(File::Share::Read)
										 .Disp(File::Disp::OpenAlways)
										 .FlagsAttrs(flags));
		
		m_cachedBy = cachedBy;
		m_fileSize = GetSize();
	}


	void StorageFile::CheckOldPathsAndRename()
	{
		bool anyOldPathExists {};
		Seq oldPathThatExists;
		for (Str const& oldPath : m_oldFullPaths)
			if (File::Exists_NotDirectory(oldPath))
				if (anyOldPathExists)
					EnsureFailWithDesc(OnFail::Abort, "A storage file exists under more than one old file name", __FUNCTION__, __LINE__);
				else
				{
					anyOldPathExists = true;
					oldPathThatExists = oldPath;
				}

		if (anyOldPathExists)
		{
			if (File::Exists_NotDirectory(m_fullPath))
				EnsureFailWithDesc(OnFail::Abort, "A storage file exists under both an old and a new file name", __FUNCTION__, __LINE__);

			File::Move(oldPathThatExists, m_fullPath);
		}
	}


	void StorageFile::ReadPages(void* firstPage, sizet nrPages, uint64 offset)
	{
		EnsureAbort((offset % Storage_PageSize) == 0);
		EnsureAbort(nrPages < (MAXDWORD / Storage_PageSize));
		DWORD bytesToRead = ((DWORD) nrPages) * Storage_PageSize;
		ReadInner(firstPage, bytesToRead, offset);
	}


	void StorageFile::ReadBytesUnaligned(void* pDestination, sizet bytesToRead, uint64 offset)
	{
		EnsureAbort(m_cachedBy == CachedBy::OS);
		EnsureAbort(bytesToRead < MAXDWORD);
		ReadInner(pDestination, (DWORD) bytesToRead, offset);
	}


	void StorageFile::ReadInner(void* pDestination, DWORD bytesToRead, uint64 offset)
	{

		DWORD bytesRead = 0;
		if (offset < m_fileSize)
		{
			if (offset != m_lastOffset)
			{
				LARGE_INTEGER li;
				li.QuadPart = NumCast<LONGLONG>(offset);
				if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
					{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
			}

			if (!ReadFile(m_hFile, pDestination, bytesToRead, &bytesRead, 0))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "ReadFile")); }
		}

		if (bytesRead != bytesToRead)
		{
			EnsureAbort(bytesRead <= bytesToRead);
			byte* pZero = ((byte*) pDestination) + bytesRead;
			sizet nZero = bytesToRead - bytesRead;
			EnsureThrow(pZero + nZero == ((byte*) pDestination) + bytesToRead);
			Mem::Zero(pZero, nZero);
		}

		m_lastOffset = offset + bytesRead;
	}


	void StorageFile::WritePages(void const* firstPage, sizet nrPages, uint64 offset)
	{
		EnsureAbort(offset <= m_fileSize);
		EnsureAbort((offset % Storage_PageSize) == 0);
		EnsureAbort(nrPages < (MAXDWORD / Storage_PageSize));

		if (offset != m_lastOffset)
		{
			LARGE_INTEGER li;
			li.QuadPart = NumCast<LONGLONG>(offset);
			if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
		}

		DWORD bytesToWrite = ((DWORD) nrPages) * Storage_PageSize;
		DWORD bytesWritten = 0;
		if (!WriteFile(m_hFile, firstPage, bytesToWrite, &bytesWritten, 0))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "WriteFile")); }
		EnsureAbort(bytesWritten == bytesToWrite);

		m_lastOffset = offset + bytesWritten;

		if (m_fileSize < m_lastOffset)
			m_fileSize = m_lastOffset;
	}


	void StorageFile::SetEof(uint64 offset)
	{
		EnsureAbort((offset % Storage_PageSize) == 0);

		if (offset != m_lastOffset)
		{
			LARGE_INTEGER li;
			li.QuadPart = NumCast<LONGLONG>(offset);
			if (!SetFilePointerEx(m_hFile, li, 0, FILE_BEGIN))
				{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetFilePointerEx")); }
		}

		if (!SetEndOfFile(m_hFile))
			{ LastWinErr e; throw e.Make<>(DescribeFileError(__FUNCTION__, "SetEndOfFile")); }

		m_lastOffset = offset;
		m_fileSize = offset;
	}



	// EntryFile

	void EntryFile::Open()
	{
		EnsureAbort(m_id != UINT_MAX);
		EnsureAbort(m_objSizeMin <= m_objSizeMax);
		EnsureAbort(m_objSizeMax >= 8);
		EnsureAbort(IsPowerOf2(m_objSizeMax));
		EnsureAbort((m_objSizeMin <= Storage_PageSize) == (m_objSizeMax <= Storage_PageSize));

		CachedBy cachedBy = OneOrMoreEntriesPerPage() ? CachedBy::Store : CachedBy::OS;
		StorageFile::OpenInner(cachedBy);
	}



	// JournalFile

	JournalFile::~JournalFile()
	{
		if (m_clearPage)
			m_pageAllocator->ReleasePage(m_clearPage);
	}


	void JournalFile::Clear()
	{
		EnsureAbort(m_hFile != INVALID_HANDLE_VALUE);
		EnsureAbort(m_pageAllocator != nullptr);
	
		if (!m_clearPage)
			m_clearPage = m_pageAllocator->GetPage();

		WritePages(m_clearPage, 1, 0);
		SetEof(Storage_PageSize);
	}



	// ObjId

	ObjId ObjId::None(0, 0);
	ObjId ObjId::Root(1, 0);


	void ObjId::EncodeBin(Enc& enc) const
	{
		Enc::Write write = enc.IncWrite(16);
		memcpy(write.Ptr(),   &m_index,    8);
		memcpy(write.Ptr()+8, &m_uniqueId, 8);
		write.Add(16);
	}


	bool ObjId::DecodeBin(Seq& s)
	{
		if (s.n < 16)
			return false;

		memcpy(&m_index,    s.p,   8);
		memcpy(&m_uniqueId, s.p+8, 8);
		s.DropBytes(16);
		return true;
	}


	bool ObjId::SkipDecodeBin(Seq& s)
	{
		if (s.n < 16)
			return false;

		s.DropBytes(16);
		return true;
	}


	void ObjId::EncObj(Enc& s) const
	{
		     if (*this == ObjId::None) s.Add("[None]");
		else if (*this == ObjId::Root) s.Add("[Root]");
		else                           s.Add("[").UInt(m_uniqueId).Add(".").UInt(m_index).Add("]");
	}


	bool ObjId::ReadStr(Seq& s)
	{
		m_uniqueId = 0;
		m_index = 0;

		if (s.StartsWithInsensitive("[None]"))
		{
			s.DropBytes(6);
			return true;
		}

		if (s.StartsWithInsensitive("[Root]"))
		{
			m_uniqueId = 1;

			s.DropBytes(6);
			return true;
		}

		Seq reader { s };
		if (reader.ReadByte() == '[')
		{
			uint64 u = reader.ReadNrUInt64Dec();
			if (reader.ReadByte() == '.')
			{
				uint64 i = reader.ReadNrUInt64Dec();
				if (reader.ReadByte() == ']')
				{
					m_uniqueId = u;
					m_index = i;

					s = reader;
					return true;
				}
			}
		}

		return false;
	}



	// Storage

	void Storage::SetDirectory(Seq path)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->SetDirectory(path);
	}

	void Storage::SetOpenOversizeFilesTarget(sizet target)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->SetOpenOversizeFilesTarget(target);
	}

	void Storage::SetCachedPagesTarget(sizet target)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->SetCachedPagesTarget(target);
	}

	void Storage::SetVerifyCommits(bool verifyCommits)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->SetVerifyCommits(verifyCommits);
	}

	void Storage::SetWritePlanTest(bool enable, uint32 writeFailOdds)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->SetWritePlanTest(enable, writeFailOdds);
	}

	void Storage::Init()
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->Init();
	}

	bool Storage::HaveTx()
	{
		EnsureThrow(m_defImpl != nullptr);
		return m_defImpl->HaveTx();
	}

	void Storage::RunTx(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->RunTx(stopCtl, txType, txFunc);
	}

	void Storage::RunTxExclusive(std::function<void()> txFunc)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->RunTxExclusive(txFunc);
	}

	bool Storage::TryRunTxNonExclusive(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc, TryRunTxParams params)
	{
		EnsureThrow(m_defImpl != nullptr);
		return m_defImpl->TryRunTxNonExclusive(stopCtl, txType, txFunc, params);
	}

	Storage::Stats Storage::GetStats(Stats::Action action)
	{
		EnsureThrow(m_defImpl != nullptr);
		return m_defImpl->GetStats(action);
	}

	void Storage::AddPostAbortAction(std::function<void()> action)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->AddPostAbortAction(action);
	}

	void Storage::AddPostCommitAction(std::function<void()> action)
	{
		EnsureThrow(m_defImpl != nullptr);
		m_defImpl->AddPostCommitAction(action);
	}



	// ObjectStore::Locator

	void ObjectStore::Locator::Set(StorageFile* sf, uint64 offset)
	{
		EnsureAbort(sf->Id() <= 255);
		EnsureAbort((sf->Id() & FileId::SpecialBit) == 0);

		m_fileId = (byte) sf->Id();
		m_oversizeFileId = ObjId();
		m_offset = offset;
	}


	void ObjectStore::Locator::Set(EOversize, ObjId oversizeFileId)
	{
		m_fileId = FileId::Oversize;
		m_oversizeFileId = oversizeFileId;
		m_offset = 0;
	}


	bool ObjectStore::Locator::operator< (Locator const& x) const
	{
		return
			(m_fileId < x.m_fileId) ||
			(m_fileId == x.m_fileId &&
				((m_fileId != FileId::Oversize && m_offset < x.m_offset) ||
				 (m_fileId == FileId::Oversize && (m_oversizeFileId < x.m_oversizeFileId ||
					(m_oversizeFileId == x.m_oversizeFileId && m_offset < x.m_offset)))));
	}


	bool ObjectStore::Locator::operator== (Locator const& x) const
	{
		return
			(m_fileId == x.m_fileId) &&
			(m_fileId != FileId::Oversize || m_oversizeFileId == x.m_oversizeFileId) &&
			(m_offset == x.m_offset);
	}



	// ObjectStore::WritePlanEncoder

	void ObjectStore::WritePlanEncoder::Encode(void const* p, sizet n)
	{
		EnsureAbort(n <= m_nrBytesRemaining);
		Mem::Copy(m_pWrite, (byte const*) p, n);
		m_pWrite += n;
		m_nrBytesRemaining -= n;
	}



	// ObjectStore

	ObjectStore::~ObjectStore()
	{
		if (m_tlsIndex != TLS_OUT_OF_INDEXES)
			EnsureReportWithCode(TlsFree(m_tlsIndex), GetLastError());
	}



	// Initialization

	void ObjectStore::SetDirectory(Seq path)
	{
		EnsureThrow(!m_inited);
		m_dir = path;
	}


	void ObjectStore::SetOpenOversizeFilesTarget(sizet target)
	{
		EnsureThrow(!m_inited);
		m_openOversizeFilesTarget = target;
	}


	void ObjectStore::SetCachedPagesTarget(sizet target)
	{
		EnsureThrow(!m_inited);
		m_cachedPagesTarget = target;
	}


	void ObjectStore::SetVerifyCommits(bool verifyCommits)
	{
		EnsureThrow(!m_inited);
		m_verifyCommits = verifyCommits;
	}


	void ObjectStore::SetWritePlanTest(bool enable, uint32 writeFailOdds)
	{
		EnsureThrow(!m_inited);
		EnsureThrow(writeFailOdds >= 1);
		m_writePlanTest = enable;
		m_writeFailOdds = writeFailOdds;
	}


	void ObjectStore::Init()
	{
		EnsureThrow(!m_inited);
		EnsureThrow(m_dir.Any());

		m_tlsIndex = TlsAlloc();
		EnsureAbort(m_tlsIndex != TLS_OUT_OF_INDEXES);

		// This implementation is not designed for very large memory page sizes
		EnsureAbort(m_pageAllocator.PageSize() == Storage_PageSize);

		CreateDirectoryIfNotExists(m_dir,                DirSecurity::Restricted_FullAccess);
		CreateDirectoryIfNotExists(GetOversizeDirPath(), DirSecurity::Restricted_FullAccess);

		// Open storage files
		sizet objSizeMin = 0;
		sizet objSizeMax = 32;

		m_writePlanFileSizes.Resize(FileId::MaxNonSpecial + 1, UINT64_MAX);

		m_metaFile.SetId(FileId::Meta);
		m_metaFile.SetFullPath(JoinPath(m_dir, "Meta.dat"));
		m_metaFile.Open();
		m_writePlanFileSizes[FileId::Meta] = m_metaFile.FileSize();

		m_indexFile.SetId(FileId::Index);
		m_indexFile.SetFullPath(JoinPath(m_dir, "Index.dat"));
		m_indexFile.SetObjSizeMin(IndexEntryBytes);
		m_indexFile.SetObjSizeMax(IndexEntryBytes);
		m_indexFile.Open();
		m_writePlanFileSizes[FileId::Index] = m_indexFile.FileSize();
		m_touchedObjects.SetNrBuckets(NumCast<sizet>(PickMax<uint64>(1000, m_indexFile.FileSize() / (IndexEntryBytes * 8))));

		m_indexFreeFile.SetId(FileId::IndexFree);
		m_indexFreeFile.SetFullPath(JoinPath(m_dir, "IndexFree.dat"));
		m_indexFreeFile.SetObjSizeMin(8);
		m_indexFreeFile.SetObjSizeMax(8);
		m_indexFreeFile.Open();
		m_writePlanFileSizes[FileId::IndexFree] = m_indexFreeFile.FileSize();

		for (uint i=0; i!=NrDataFiles; ++i)
		{
			DataFile& df = m_dataFiles[i];
			FreeFile& ff = m_dataFreeFiles[i];

			Str objSizeMaxStr8, objSizeMaxStr4;
			objSizeMaxStr8.UInt(objSizeMax, 10, 8);
			objSizeMaxStr4.UInt(objSizeMax, 10, 4);

			df.SetId(NumCast<byte>(FileId::DataStart + i));
			if (objSizeMaxStr8 != objSizeMaxStr4)
				df.AddOldFullPath(JoinPath(m_dir, Str("Store").Add(objSizeMaxStr4).Add(".dat")));
			df.SetFullPath(JoinPath(m_dir, Str("Store").Add(objSizeMaxStr8).Add(".dat")));
			df.SetObjSizeMin(objSizeMin);
			df.SetObjSizeMax(objSizeMax);
			df.Open();
			m_writePlanFileSizes[df.Id()] = df.FileSize();

			ff.SetId(NumCast<byte>(FileId::DataFreeStart + i));
			if (objSizeMaxStr8 != objSizeMaxStr4)
				ff.AddOldFullPath(JoinPath(m_dir, Str("Store").Add(objSizeMaxStr4).Add("Free.dat")));
			ff.SetFullPath(JoinPath(m_dir, Str("Store").Add(objSizeMaxStr8).Add("Free.dat")));
			ff.SetObjSizeMin(8);
			ff.SetObjSizeMax(8);
			ff.Open();
			m_writePlanFileSizes[ff.Id()] = ff.FileSize();

			objSizeMin = objSizeMax + 1;
			objSizeMax = objSizeMax * 2;
		}

		m_journalFile.SetId(FileId::Journal);
		m_journalFile.SetPageAllocator(m_pageAllocator);
		m_journalFile.SetFullPath(JoinPath(m_dir, "Journal.dat"));
		m_journalFile.Open();

		// Need meta data before completing any unfinished writes in journal
		LoadMetaData();

		// Check journal file for unfinished writes
		if (CompleteWritePlanFromJournal())
		{
			// Need to reload meta data
			LoadMetaData();
		}

		// Initialize free index state information
		LoadFisPages();

		m_inited = true;
	}


	void ObjectStore::RunTx(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc)
	{
		if (!TryRunTxNonExclusive(stopCtl, txType, txFunc))
			RunTxExclusive(txFunc);
	}


	void ObjectStore::RunTxExclusive(std::function<void()> txFunc)
	{
		// Prevent new transactions from starting
		Locker startLocker(m_mxStart);

		// Wait for other transactions to finish
		while (true)
		{
			// Check if all transactions have finished
			{
				Locker locker(m_mx);
				if (!m_nrActiveTransactions)
				{
					m_needNoTxNotification = false;
					break;
				}

				m_needNoTxNotification = true;
			}

			// Wait for notification to check again
			Wait1(m_noTxNotificationEvent.Handle(), INFINITE);
		}

		InterlockedIncrement(&m_stats.m_nrRunTxExclusive);

		// Run transaction exclusively. If it fails, it fails
		StartTx(TxScheduler::InvalidStxId);

		try { txFunc(); }
		catch (...)
		{
			AbortTx();
			throw;
		}

		CommitTx();
	}


	bool ObjectStore::TryRunTxNonExclusive(Rp<StopCtl> const& stopCtl, TypeIndex txType, std::function<void()> txFunc, TryRunTxParams params)
	{
		EnsureThrow(stopCtl.Any());
		if (stopCtl->StopEvent().IsSignaled())
			throw ExecutionAborted();

		if (params.m_maxAttempts < 1)
			params.m_maxAttempts = 1;
		if (params.m_minWaitMs > params.m_maxWaitMs)
			params.m_minWaitMs = params.m_maxWaitMs;

		InterlockedIncrement(&m_stats.m_nrTryRunTxNonExclusive);

		DWORD nextWaitMs = params.m_minWaitMs;
		sizet attemptNr = 1;
		while (true)
		{
			uint64 stxId = m_txScheduler.TxS_Start(stopCtl, txType);
			OnExit onExitNoConflict = [&] { m_txScheduler.TxS_NoConflict(stxId); };

			StartTx(stxId);

			try
			{
				try { txFunc(); }
				catch (...)
				{
					AbortTx();
					throw;
				}

				CommitTx();
				return true;
			}
			catch (RetryTxException const& e)
			{
				onExitNoConflict.Dismiss();

				sizet retryIndex = PickMin<sizet>(attemptNr, Stats::MaxRetriesTracked) - 1;
				InterlockedIncrement(&m_stats.m_nrNonExclusiveRetries[retryIndex]);

				m_txScheduler.TxS_Conflict(stxId, e.m_conflictTx);

				if (Wait1(stopCtl->StopEvent().Handle(), nextWaitMs) == 0)
					throw ExecutionAborted();
			
				if (!nextWaitMs)
					nextWaitMs = 1;

				nextWaitMs *= params.m_waitMultiplyFactor;

				if (nextWaitMs > params.m_maxWaitMs)
					nextWaitMs = params.m_maxWaitMs;

				if (++attemptNr > params.m_maxAttempts)
				{
					InterlockedIncrement(&m_stats.m_nrNonExclusiveGiveUps);
					return false;
				}
			}
		}
	}


	Storage::Stats ObjectStore::GetStats(Stats::Action action)
	{
		auto getField = [action] (sizet& dest, sizet volatile& src)
			{
				if (action == Stats::Keep)
					dest = InterlockedExchangeAdd(&src, 0);
				else
					dest = InterlockedExchange(&src, 0);
			};

		Stats stats;

		{
			Locker locker { m_mx };

			getField(stats.m_nrRunTxExclusive,       m_stats.m_nrRunTxExclusive       );
			getField(stats.m_nrTryRunTxNonExclusive, m_stats.m_nrTryRunTxNonExclusive );
			getField(stats.m_nrNonExclusiveGiveUps,  m_stats.m_nrNonExclusiveGiveUps  );
			getField(stats.m_nrStartTx,              m_stats.m_nrStartTx              );
			getField(stats.m_nrCommitTx,             m_stats.m_nrCommitTx             );
			getField(stats.m_nrAbortTx,              m_stats.m_nrAbortTx              );

			for (sizet i=0; i!=Stats::MaxRetriesTracked; ++i)
				getField(stats.m_nrNonExclusiveRetries[i], m_stats.m_nrNonExclusiveRetries[i]);
		}

		return stats;
	}


	void ObjectStore::StartTx(uint64 stxId)
	{
		Locker lockerStart(m_mxStart);
		Locker locker(m_mx);
		if (m_tainted)
			throw Tainted();

		EnsureAbort(TlsGetValue(m_tlsIndex) == 0);

		++m_stats.m_nrStartTx;

		// Find place for transaction in vector
		sizet i = 0;
		while (true)
		{
			if (i == m_transactions.Len())
			{
				m_transactions.Add().Set(new Tx);
				break;
			}

			if (!m_transactions[i].Any())
			{
				m_transactions[i].Set(new Tx);
				break;
			}

			++i;
		}

		// Initialize transaction state
		Rp<Tx> tx = m_transactions[i];
		tx->m_stxId = stxId;
		tx->m_state = Tx::InProgress;
		tx->m_transactionsIndex = i;
		tx->m_txNr = ++m_lastTxNr;

		EnsureAbort(TlsSetValue(m_tlsIndex, tx.Ptr()));
		tx->AddRef();

		++m_nrActiveTransactions;
	}


	void ObjectStore::AbortTx()
	{
		std::vector<std::function<void()>> postAbortActions;

		{
			Locker locker(m_mx);
			if (m_tainted)
				throw Tainted();

			Tx* tx = (Tx*) TlsGetValue(m_tlsIndex);
			EnsureAbort(tx != nullptr);
			EnsureAbort(tx->m_state == Tx::InProgress);
			EnsureAbort(tx->m_transactionsIndex <= m_transactions.Len());
			EnsureAbort(m_transactions[tx->m_transactionsIndex].Ptr() == tx);

			++m_stats.m_nrAbortTx;

			postAbortActions.swap(tx->m_postAbortActions);
			AbortTx_Inner(tx);
		}

		for (std::function<void()> const& action : postAbortActions)
		{
			try { action(); }
			catch (std::exception const& e) { EnsureFailWithDesc(OnFail::Abort, e.what(), __FUNCTION__, __LINE__); }
		}
	}


	void ObjectStore::CommitTx()
	{
		std::vector<std::function<void()>> postCommitActions;

		{
			Locker locker { m_mx };
			if (m_tainted)
				throw Tainted();

			Tx* tx = (Tx*) TlsGetValue(m_tlsIndex);
			EnsureAbort(tx != nullptr);
			EnsureAbort(tx->m_state == Tx::InProgress);
			EnsureAbort(tx->m_transactionsIndex <= m_transactions.Len());
			EnsureAbort(m_transactions[tx->m_transactionsIndex].Ptr() == tx);

			++m_stats.m_nrCommitTx;

			bool anyChanges = (tx->m_objectsToInsert.Any() || tx->m_objectsToReplace.Any() || tx->m_objectsToRemove.Any());
			if (anyChanges)
			{
				uint64 commitNr { ++m_lastCommitNr };

				// Verify that reads from objects have not been invalidated by other commits
				for (RetrievedObject const& retrievedObject : tx->m_retrievedObjects)
					if (retrievedObject.Changed())
					{
						uint64 conflictStxId = retrievedObject.m_touchedObject->m_commitStxId;
						AbortTx_Inner(tx);
						throw RetryTxException(m_txScheduler, conflictStxId, "CommitTx: Read invalidated by a commit later than the one previously read");
					}

				RunWritePlan( [&] { CommitTx_MakeWritePlan(tx, commitNr); } );

				if (m_verifyCommits)
				{
					for (TouchedObject* obj : tx->m_objectsToInsert ) CommitTx_VerifyObject(obj);
					for (TouchedObject* obj : tx->m_objectsToReplace) CommitTx_VerifyObject(obj);
					for (TouchedObject* obj : tx->m_objectsToRemove ) CommitTx_VerifyObject(obj);
				}
			}
			
			postCommitActions.swap(tx->m_postCommitActions);
			EndTxCommon(tx);
		}

		for (std::function<void()> const& action : postCommitActions)
		{
			try { action(); }
			catch (std::exception const& e) { EnsureFailWithDesc(OnFail::Abort, e.what(), __FUNCTION__, __LINE__); }
		}
	}


	void ObjectStore::EndTxCommon(Tx* tx)
	{
		for (TouchedObject* obj : tx->m_objectsToInsert     ) DecrementTouchedObjectRefCount(obj);
		for (TouchedObject* obj : tx->m_objectsToReplace    ) DecrementTouchedObjectRefCount(obj);
		for (TouchedObject* obj : tx->m_objectsToRemove     ) DecrementTouchedObjectRefCount(obj);
		for (TouchedObject* obj : tx->m_objectsToKeepAround ) DecrementTouchedObjectRefCount(obj);

		m_transactions[tx->m_transactionsIndex].Clear();
		tx->m_transactionsIndex = SIZE_MAX;

		PruneCache();

		EnsureAbort(TlsSetValue(m_tlsIndex, 0));
		tx->Release();

		EnsureAbort(m_nrActiveTransactions > 0);
		if (!--m_nrActiveTransactions)
		{
			m_transactions.Clear();
			if (m_needNoTxNotification)
				m_noTxNotificationEvent.Signal();
		}
	}


	void ObjectStore::AbortTx_Inner(Tx* tx)
	{
		// Clear transaction state
		for (TouchedObject* tob : tx->m_objectsToReplace)
			if (tob->m_uncommittedTxNr != 0)
			{
				EnsureAbort(tob->m_uncommittedTxNr == tx->m_txNr);
				tob->ClearUncommittedAction();
			}

		for (TouchedObject* tob : tx->m_objectsToRemove)
			if (tob->m_uncommittedTxNr != 0)
			{
				EnsureAbort(tob->m_uncommittedTxNr == tx->m_txNr);
				tob->ClearUncommittedAction();
			}

		EndTxCommon(tx);
	}


	void ObjectStore::CommitTx_MakeWritePlan(Tx* tx, uint64 commitNr)
	{
		sizet nrIndicesFreed {};
		sizet nrIndicesUsed  {};

		nrIndicesUsed = CommitTx_InsertObjects(tx, commitNr);
		CommitTx_ReplaceObjects(tx, commitNr);
		nrIndicesFreed = CommitTx_RemoveObjects(tx, commitNr);

		if (nrIndicesUsed > nrIndicesFreed)
			ConsolidateFreeIndexState();

		uint64* mfPage { (uint64*) CachedReadPage(&m_metaFile, 0) };
		mfPage[0] = m_lastUniqueId;
		m_lastWrittenUniqueId = m_lastUniqueId;

		Locator mfLocator { &m_metaFile, 0 };
		AddWritePlanEntry_CachedPage(mfLocator, mfPage);
	}


	sizet ObjectStore::CommitTx_InsertObjects(Tx* tx, uint64 commitNr)
	{
		// Insert objects to be inserted
		sizet nrIndicesUsed {};
		OrderedSet<uint64> indicesUsed;
		OrderedSet<uint64> indicesRestored;

		for (TouchedObject* tob : tx->m_objectsToInsert)
		{
			EnsureAbort(tob->m_uncommittedTxNr == tx->m_txNr);

			if (tob->m_uncommittedAction != ObjectAction::Insert)
			{
				EnsureAbort(tob->m_uncommittedAction == ObjectAction::Remove);
				indicesRestored.Add(tob->m_objId.m_index);

				// Objects that were removed after being inserted by same transaction are not added to tx->m_objectsToRemove.
				// We update object state here.
				tob->m_committedState = ObjectState::Removed;
				tob->m_committedData.Clear();
				tob->m_commitNr = commitNr;
				tob->m_commitStxId = tx->m_stxId;
				tob->ClearUncommittedAction();
			}
			else
			{
				tob->m_committedState = ObjectState::Loaded;
				tob->m_committedData  = tob->m_uncommittedData;
				tob->m_commitNr       = commitNr;
				tob->m_commitStxId    = tx->m_stxId;
				tob->ClearUncommittedAction();

				EnsureAbort(tob->m_committedData.Any());
				Seq data = tob->m_committedData.Ref();

				// Identify data file to put the object into
				DataFile* df {};
				FreeFile* ff {};
				uint64    compactLocator {};
				if (FindDataFileByObjectSizeNoLen(data.n, df, ff))
				{
					uint64 offset { UINT64_MAX };
					CommitTx_InsertObjectData(data, df, ff, offset);						
					compactLocator = MakeCompactLocator(df->Id(), offset);
				}
				else
				{
					// Oversize object, to be stored separately
					Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, 4 + data.n);
					((uint32*) writePages->Ptr())[0] = NumCast<uint32>(data.n);
					Mem::Copy(writePages->Ptr() + 4, data.p, data.n);

					Locator locator { Locator::Oversize, tob->m_objId };
					AddWritePlanEntry_SequentialPages(locator, writePages, WritePlanEntry::WriteEof);

					compactLocator = MakeCompactLocator(FileId::Oversize, 0);
				}

				// Update object location in index
				uint64           ifOffset   { tob->m_objId.m_index * IndexEntryBytes };
				StructuredOffset structured = GetStructuredOffset(ifOffset);
				byte*            ifPage     { CachedReadPage(&m_indexFile, structured.pageOffsetInFile) };
				uint64*          ifEntry    { (uint64*) (ifPage + structured.offsetInPage) };

				EnsureAbort(ifEntry[0] == 0);
				ifEntry[0] = tob->m_objId.m_uniqueId;
				ifEntry[1] = compactLocator;

				Locator ifLocator(&m_indexFile, structured.pageOffsetInFile);
				AddWritePlanEntry_CachedPage(ifLocator, ifPage);
		
				indicesUsed.Add(tob->m_objId.m_index);
				++nrIndicesUsed;
			}
		}

		// Update free index state
		if (indicesUsed.Any() || indicesRestored.Any())
		{
			uint64 iffSize { GetWritePlanFileSize(&m_indexFreeFile) };
			EnsureAbort(iffSize == m_fisPages.Len() * Storage_PageSize);
			EnsureAbort(m_fisPages.Any());
			
			sizet iffPageOffset {};
			for (sizet fisPageIndex=0; ; ++fisPageIndex, iffPageOffset += Storage_PageSize)
			{
				// If state is consistent, shouldn't exhaust free index pages before finding last index
				EnsureAbort(fisPageIndex < m_fisPages.Len());

				FisPage& fisPage { m_fisPages[fisPageIndex] };
				if (fisPage.m_nrReserved > 0)
				{
					// This page contains some reserved entries. Find them and remove them.
					uint64* iffPage { (uint64*) CachedReadPage(&m_indexFreeFile, iffPageOffset) };
					bool iffPageModified {};

					for (sizet i=0; i!=UInt64PerPage; ++i)
					{
						OrderedSet<uint64>::It itUsed     { indicesUsed    .Find(iffPage[i]) };
						OrderedSet<uint64>::It itRestored { indicesRestored.Find(iffPage[i]) };

						if (itUsed.Any())
						{
							EnsureAbort(!itRestored.Any());
							EnsureAbort(fisPage.m_state[i] == FreeIndexState::Reserved);
							fisPage.m_state[i] = FreeIndexState::Invalid;
							--(fisPage.m_nrReserved);
							++(fisPage.m_nrInvalid);
							++m_fisPages_totalNrInvalid;

							iffPage[i] = UINT64_MAX;
							iffPageModified = true;

							indicesUsed.Erase(itUsed);
						}
						else if (itRestored.Any())
						{
							EnsureAbort(fisPage.m_state[i] == FreeIndexState::Reserved);
							fisPage.m_state[i] = FreeIndexState::Available;
							--(fisPage.m_nrReserved);
							++(fisPage.m_nrAvailable);

							indicesRestored.Erase(itRestored);
						}

						if (!indicesUsed.Any() && !indicesRestored.Any())
							break;
					}

					if (iffPageModified)
					{
						Locator iffLocator { &m_indexFreeFile, iffPageOffset };
						AddWritePlanEntry_CachedPage(iffLocator, iffPage); 
					}

					if (!indicesUsed.Any() && !indicesRestored.Any())
						break;
				}
			}
		}

		return nrIndicesUsed;
	}


	void ObjectStore::CommitTx_ReplaceObjects(Tx* tx, uint64 commitNr)
	{
		// Replace objects to be replaced
		for (TouchedObject* tob : tx->m_objectsToReplace)
		{
			EnsureAbort(tob->m_uncommittedTxNr == tx->m_txNr);
		
			if (tob->m_uncommittedAction != ObjectAction::Replace)
			{
				EnsureAbort(tob->m_uncommittedAction == ObjectAction::Remove);
			}
			else
			{
				tob->m_committedState = ObjectState::Loaded;
				tob->m_committedData  = tob->m_uncommittedData;
				tob->m_commitNr       = commitNr;
				tob->m_commitStxId    = tx->m_stxId;
				tob->ClearUncommittedAction();

				uint64           ifOffset           { tob->m_objId.m_index * IndexEntryBytes };
				StructuredOffset ifStructured       = GetStructuredOffset(ifOffset);
				byte*            ifPage             { CachedReadPage(&m_indexFile, ifStructured.pageOffsetInFile) };
				uint64*          ifEntry            { (uint64*) (ifPage + ifStructured.offsetInPage) };
				EnsureAbort(ifEntry[0] == tob->m_objId.m_uniqueId);
				uint64           prevCompactLocator { ifEntry[1] };
				byte             prevFileId         { GetCompactLocatorFileId(prevCompactLocator) };
				uint64           prevOffset         { GetCompactLocatorOffset(prevCompactLocator) };

				// Identify new data file to put the object into
				bool      ifEntryChanged {};
				DataFile* df             {};
				FreeFile* ff             {};
				Seq       data           { tob->m_committedData.Ref() };

				if (FindDataFileByObjectSizeNoLen(data.n, df, ff))
				{
					// Is it the same data file?
					if (df->Id() == prevFileId)
					{
						// Update object data in place
						CommitTx_WriteObjectData(data, df, prevOffset);
					}
					else
					{
						if (prevFileId != FileId::Oversize)
						{
							// Remove object from previous data file
							CommitTx_RemoveObjectData(prevFileId, prevOffset);
						}
						else
						{
							// Remove previous oversize file
							Locator ofLocator { Locator::Oversize, tob->m_objId };
							AddWritePlanEntry_DeleteOversizeFile(ofLocator);
						}

						// Write object to new data file
						uint64 offset { UINT64_MAX };
						CommitTx_InsertObjectData(data, df, ff, offset);
					
						ifEntry[1] = MakeCompactLocator(df->Id(), offset);
						ifEntryChanged = true;
					}
				}
				else
				{
					// Oversize object, to be stored separately
					if (prevFileId != FileId::Oversize)
					{
						// Remove object from previous data file
						CommitTx_RemoveObjectData(prevFileId, prevOffset);

						ifEntry[1] = MakeCompactLocator(FileId::Oversize, 0);
						ifEntryChanged = true;
					}

					// Write object data to oversize file
					Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, 4 + data.n);
					((uint32*) writePages->Ptr())[0] = NumCast<uint32>(data.n);
					Mem::Copy(writePages->Ptr() + 4, data.p, data.n);

					Locator ofLocator { Locator::Oversize, tob->m_objId };
					AddWritePlanEntry_SequentialPages(ofLocator, writePages, WritePlanEntry::WriteEof);
				}

				if (ifEntryChanged)
				{
					Locator ifLocator { &m_indexFile, ifStructured.pageOffsetInFile };
					AddWritePlanEntry_CachedPage(ifLocator, ifPage);
				}
			}
		}
	}


	sizet ObjectStore::CommitTx_RemoveObjects(Tx* tx, uint64 commitNr)
	{
		sizet nrIndicesFreed {};
		Vec<uint64> indicesFreed;

		for (TouchedObject* tob : tx->m_objectsToRemove)
		{
			EnsureAbort(tob->m_uncommittedTxNr == tx->m_txNr);
			EnsureAbort(tob->m_uncommittedAction == ObjectAction::Remove);

			tob->m_committedState = ObjectState::Removed;
			tob->m_committedData.Clear();
			tob->m_commitNr    = commitNr;
			tob->m_commitStxId = tx->m_stxId;
			tob->ClearUncommittedAction();

			// Find the object's index record
			uint64           ifOffset   { tob->m_objId.m_index * IndexEntryBytes };
			StructuredOffset structured = GetStructuredOffset(ifOffset);
			byte*            ifPage     { CachedReadPage(&m_indexFile, structured.pageOffsetInFile) };
			uint64*          ifEntry    { (uint64*) (ifPage + structured.offsetInPage) };
		
			// If the unique ID no longer matches, the object is already gone
			if (ifEntry[0] == tob->m_objId.m_uniqueId)
			{
				uint64 prevCompactLocator { ifEntry[1] };
				byte   fileId { GetCompactLocatorFileId(prevCompactLocator) };
				uint64 offset { GetCompactLocatorOffset(prevCompactLocator) };

				if (fileId == FileId::Oversize)
				{
					// Remove oversize file containing object data
					Locator ofLocator { Locator::Oversize, tob->m_objId };
					AddWritePlanEntry_DeleteOversizeFile(ofLocator);
				}
				else
				{
					// Remove object data from regular storage file
					CommitTx_RemoveObjectData(fileId, offset);
				}

				// Free index entry
				ifEntry[0] = 0;
				ifEntry[1] = 0;

				Locator ifLocator { &m_indexFile, structured.pageOffsetInFile };
				AddWritePlanEntry_CachedPage(ifLocator, ifPage);

				indicesFreed.Add(tob->m_objId.m_index);
				++nrIndicesFreed;
			}
		}

		// Update free index state
		if (indicesFreed.Any())
		{
			// Try to find vacant records in free index state file
			uint64 iffSize { GetWritePlanFileSize(&m_indexFreeFile) };
			EnsureAbort(iffSize == m_fisPages.Len() * Storage_PageSize);

			sizet iffPageOffset {};
			for (sizet fisPageIndex=0; indicesFreed.Any() && fisPageIndex!=m_fisPages.Len(); ++fisPageIndex, iffPageOffset += Storage_PageSize)
			{
				FisPage& fisPage { m_fisPages[fisPageIndex] };
				if (fisPage.m_nrInvalid > 0)
				{
					uint64* iffPage     { (uint64*) CachedReadPage(&m_indexFreeFile, iffPageOffset) };
					bool    pageChanged {};

					for (sizet i=0; i!=UInt64PerPage; ++i)
						if (fisPage.m_state[i] == FreeIndexState::Invalid)
						{
							EnsureAbort(iffPage[i] == UINT64_MAX);

							iffPage[i] = indicesFreed.Last();
							fisPage.m_state[i] = FreeIndexState::Available;
							++(fisPage.m_nrAvailable);
							--(fisPage.m_nrInvalid);
							--m_fisPages_totalNrInvalid;
						
							indicesFreed.PopLast();
							pageChanged = true;
						
							if (!indicesFreed.Any())
								break;
						}

					if (pageChanged)
					{
						Locator iffLocator { &m_indexFreeFile, iffPageOffset };
						AddWritePlanEntry_CachedPage(iffLocator, iffPage);
					}
				}
			}

			// If we still have free indices to record, we need to add one or more new pages to the index free file.
			while (indicesFreed.Any())
			{
				uint64*  iffPage { (uint64*) CachedReadPage(&m_indexFreeFile, iffSize) };
				FisPage& fisPage { m_fisPages.Add() };

				sizet i {};
				for (; indicesFreed.Any() && i != UInt64PerPage; ++i)
				{
					iffPage[i] = indicesFreed.Last();
					indicesFreed.PopLast();

					fisPage.m_state[i] = FreeIndexState::Available;
					++(fisPage.m_nrAvailable);
				}

				for (; i!=UInt64PerPage; ++i)
				{
					iffPage[i] = UINT64_MAX;

					fisPage.m_state[i] = FreeIndexState::Invalid;
					++(fisPage.m_nrInvalid);
					++m_fisPages_totalNrInvalid;
				}

				EnsureAbort(fisPage.m_nrAvailable + fisPage.m_nrInvalid == fisPage.m_state.Len());

				Locator iffLocator { &m_indexFreeFile, iffSize };
				AddWritePlanEntry_CachedPage(iffLocator, iffPage);

				iffSize += Storage_PageSize;
			}
		}

		return nrIndicesFreed;
	}


	void ObjectStore::CommitTx_InsertObjectData(Seq data, DataFile* df, FreeFile* ff, uint64& offset)
	{
		offset = CommitTx_ClaimDataFileFreeEntryOffset(df, ff);
		CommitTx_WriteObjectData(data, df, offset);
	}


	void ObjectStore::CommitTx_WriteObjectData(Seq data, DataFile* df, uint64 offset)
	{
		if (df->IsStoreCached())
		{
			EnsureAbort(2 + data.n <= df->ObjSizeMax());
			StructuredOffset prevStructured = GetStructuredOffset(offset);
			byte* dfPage = CachedReadPage(df, prevStructured.pageOffsetInFile);
			byte* entry = dfPage + prevStructured.offsetInPage;
					
			((uint16*) entry)[0] = NumCast<uint16>(data.n);
			Mem::Copy(entry + 2, data.p, data.n);

			Locator dfLocator { df, prevStructured.pageOffsetInFile };
			AddWritePlanEntry_CachedPage(dfLocator, dfPage);
		}
		else
		{
			EnsureAbort(4 + data.n <= df->ObjSizeMax());
			Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, 4 + data.n);
			((uint32*) writePages->Ptr())[0] = NumCast<uint32>(data.n);
			Mem::Copy(writePages->Ptr() + 4, data.p, data.n);

			Locator dfLocator { df, offset };
			AddWritePlanEntry_SequentialPages(dfLocator, writePages);
		}
	}


	uint64 ObjectStore::CommitTx_ClaimDataFileFreeEntryOffset(DataFile* df, FreeFile* ff)
	{
		// Find a free offset in the free file
		uint64 ffSize = GetWritePlanFileSize(ff);
		uint64 ffOffset = ffSize;
		while (ffOffset >= Storage_PageSize)
		{
			ffOffset -= Storage_PageSize;
			uint64* page = (uint64*) CachedReadPage(ff, ffOffset);		
			for (sizet i=0; i!=UInt64PerPage; ++i)
				if (page[i] != UINT64_MAX)
				{
					uint64 dataFileFreeEntryOffset = page[i];
					page[i] = UINT64_MAX;
					Locator locator(ff, ffOffset);
					AddWritePlanEntry_CachedPage(locator, page, WritePlanEntry::WriteEof);
					return dataFileFreeEntryOffset;
				}
		}

		// No free offsets found in free file. Discard existing free file content and replace it with new free offsets
		uint64* const newFfPage = (uint64*) CachedReadPage(ff, 0);
		sizet ffPageIndex {};

		uint64 const dfSize = GetWritePlanFileSize(df);
		if (df->IsStoreCached())
		{
			sizet const dfPagesPerFfPage = df->ObjSizeMax() / 8;
			for (sizet i=0; i!=dfPagesPerFfPage; ++i)
			{
				uint64 newDfPageOffset = dfSize + (i * Storage_PageSize);
				byte* newDfPage = CachedReadPage(df, newDfPageOffset);
				AddWritePlanEntry_CachedPage(Locator(df, newDfPageOffset), newDfPage);
		
				sizet entriesPerDfPage = Storage_PageSize / df->ObjSizeMax();
				for (sizet k=0; k!=entriesPerDfPage; ++k)
					newFfPage[ffPageIndex++] = newDfPageOffset + (k * df->ObjSizeMax());
			}
			EnsureAbort(8 * ffPageIndex == Storage_PageSize);
		}
		else
		{
			sizet dfIncrementBytes = DefaultOsCachedDataFileIncrementBytes;
			if (dfIncrementBytes < df->ObjSizeMax())
				dfIncrementBytes = df->ObjSizeMax();

			EnsureAbort((dfIncrementBytes % Storage_PageSize) == 0);
			EnsureAbort((dfIncrementBytes % df->ObjSizeMax()) == 0);
			sizet const entriesAdded = dfIncrementBytes / df->ObjSizeMax();
			EnsureAbort(entriesAdded >= 1);
			EnsureAbort(entriesAdded <= UInt64PerPage);

			Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, dfIncrementBytes);
			AddWritePlanEntry_SequentialPages(Locator(df, dfSize), writePages);

			sizet k {};
			for (; k!=entriesAdded;  ++k) newFfPage[k] = dfSize + (k * df->ObjSizeMax());
			for (; k!=UInt64PerPage; ++k) newFfPage[k] = UINT64_MAX;
		}

		uint64 dataFileFreeEntryOffset = newFfPage[0];
		newFfPage[0] = UINT64_MAX;
		Locator ffLocator = Locator(ff, 0);
		AddWritePlanEntry_CachedPage(ffLocator, newFfPage, WritePlanEntry::WriteEof);
		return dataFileFreeEntryOffset;
	}


	void ObjectStore::CommitTx_RemoveObjectData(byte fileId, uint64 offset)
	{
		sizet index = (sizet) (fileId - FileId::DataStart);
		EnsureAbort(index < NrDataFiles);
		DataFile* df = &(m_dataFiles[index]);
		FreeFile* ff = &(m_dataFreeFiles[index]);
		CommitTx_RemoveObjectData(df, ff, offset);
	}


	void ObjectStore::CommitTx_RemoveObjectData(DataFile* df, FreeFile* ff, uint64 offset)
	{
		// Clear entry from data file
		if (df->IsStoreCached())
		{
			StructuredOffset structured = GetStructuredOffset(offset);
			byte* dfPage = CachedReadPage(df, structured.pageOffsetInFile);
			Mem::Zero(dfPage + structured.offsetInPage, df->ObjSizeMax());

			Locator dfLocator(df, structured.pageOffsetInFile);
			AddWritePlanEntry_CachedPage(dfLocator, dfPage);
		}
		else
		{
			Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, df->ObjSizeMax());
			Locator dfLocator { df, offset };
			AddWritePlanEntry_SequentialPages(dfLocator, writePages);
		}

		// See if there's room for the entry's offset in free file
		bool foundRoom = false;
		uint64 ffSize = GetWritePlanFileSize(ff);
		if (ffSize > 0)
		{
			EnsureAbort((ffSize % Storage_PageSize) == 0);
			uint64 ffOffset = ffSize - Storage_PageSize;
			uint64* ffPage = (uint64*) CachedReadPage(ff, ffOffset);
			for (sizet i=0; i!=UInt64PerPage; ++i)
				if (ffPage[i] == UINT64_MAX)
				{
					ffPage[i] = offset;
					foundRoom = true;
					break;
				}

			if (foundRoom)
			{
				Locator ffLocator(ff, ffOffset);
				AddWritePlanEntry_CachedPage(ffLocator, ffPage);
			}
		}

		if (!foundRoom)
		{
			// Add page to free file
			uint64* ffPage = (uint64*) CachedReadPage(ff, ffSize);
			memset(ffPage, 0xFF, Storage_PageSize);
			ffPage[0] = offset;

			Locator ffLocator(ff, ffSize);
			AddWritePlanEntry_CachedPage(ffLocator, ffPage);
		}
	}


	void ObjectStore::CommitTx_VerifyObject(TouchedObject* tob)
	{
		// Verify that state of committed object in memory corresponds to state on disk
		EnsureAbort(tob != nullptr);
		EnsureAbort(tob->m_objId != ObjId::None);
		EnsureAbort(tob->m_committedState == ObjectState::Loaded || tob->m_committedState == ObjectState::Removed);

		byte* page = m_pageAllocator.GetPage();
		OnExit releasePage( [&] () { m_pageAllocator.ReleasePage(page); } );

		uint64 ifOffset = tob->m_objId.m_index * IndexEntryBytes;
		StructuredOffset structured = GetStructuredOffset(ifOffset);
		m_indexFile.ReadPages(page, 1, structured.pageOffsetInFile);
		uint64* ifEntry = (uint64*) (page + structured.offsetInPage);

		if (tob->m_committedState == ObjectState::Removed)
		{
			EnsureAbort(ifEntry[0] == 0);
			EnsureAbort(ifEntry[1] == 0);
		}
		else
		{
			EnsureAbort(tob->m_committedData.Any());

			EnsureAbort(ifEntry[0] == tob->m_objId.m_uniqueId);
			uint64 compactLocator = ifEntry[1];
			byte fileId = GetCompactLocatorFileId(compactLocator);
			uint64 dfOffset = GetCompactLocatorOffset(compactLocator);

			if (fileId == FileId::Oversize)
			{
				// Load object data from oversize file
				StorageFile* of = GetOversizeFile(tob->m_objId);
				EnsureAbort(of != nullptr);

				SequentialPages pages { m_pageAllocator, tob->m_committedData->Len() };
				of->ReadPages(pages.Ptr(), pages.Nr(), 0);

				uint32 dataLen = ((uint32*) pages.Ptr())[0];
				EnsureAbort(dataLen == tob->m_committedData->Len());
				EnsureAbort(!memcmp(pages.Ptr() + 4, tob->m_committedData->Ptr(), dataLen));
			}
			else
			{
				// Load object data from regular storage file
				DataFile* df = FindDataFileById(fileId);
				EnsureAbort(df != nullptr);

				auto verify = [] (TouchedObject* tob, DataFile* df, byte* dfEntry, sizet lenLen, sizet dataLen)
					{
						EnsureAbort(lenLen + dataLen >= df->ObjSizeMin());
						EnsureAbort(lenLen + dataLen <= df->ObjSizeMax());
						EnsureAbort(dataLen == tob->m_committedData->Len());
						EnsureAbort(!memcmp(dfEntry + lenLen, tob->m_committedData->Ptr(), dataLen));
					};

				if (df->IsStoreCached())
				{
					structured = GetStructuredOffset(dfOffset);
					df->ReadPages(page, 1, structured.pageOffsetInFile);
					byte* dfEntry = page + structured.offsetInPage;
					sizet dataLen = ((uint16*) dfEntry)[0];
					verify(tob, df, dfEntry, 2, dataLen);
				}
				else
				{
					SequentialPages pages { m_pageAllocator, tob->m_committedData->Len() };
					df->ReadPages(pages.Ptr(), pages.Nr(), dfOffset);
					sizet dataLen = ((uint32*) pages.Ptr())[0];
					verify(tob, df, pages.Ptr(), 4, dataLen);
				}
			}
		}
	}


	ObjId ObjectStore::InsertObject(Rp<RcStr> const& data)
	{
		Locker locker { m_mx };
		if (m_tainted)
			throw Tainted();

		Tx* tx { (Tx*) TlsGetValue(m_tlsIndex) };
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);
		EnsureAbort(data.Any());

		uint64 uniqueId { ++m_lastUniqueId };
		uint64 index    { ReserveIndex() };
		ObjId  objId    { uniqueId, index };

		// A TouchedObject may already exist for this index and uniqueId combination,
		// if the application previously attempted to retrieve it
		TouchedObject* tob { FindTouchedObjectInMemory(objId) };
		if (!tob)
		{
			AutoFree<TouchedObject> autoFreeTob { new TouchedObject(objId) };
			tob = autoFreeTob.Ptr();
			InsertTouchedObject(autoFreeTob);
		}

		tob->m_committedState    = ObjectState::ToBeInserted;
		tob->m_uncommittedAction = ObjectAction::Insert;
		tob->m_uncommittedData   = data;
		tob->m_uncommittedTxNr   = tx->m_txNr;
		tob->m_uncommittedStxId  = tx->m_stxId;

		tx->m_objectsToInsert.Add(tob);
		++(tob->m_refCount);

		return objId;
	}


	Rp<RcStr> ObjectStore::RetrieveObject(ObjId objId, ObjId refObjId)
	{
		Locker locker { m_mx };
		if (m_tainted)
			throw Tainted();

		Tx* tx { (Tx*) TlsGetValue(m_tlsIndex) };
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);

		// Validate reference object
		if (refObjId != ObjId::None)
		{
			EnsureThrow(refObjId != objId);
			RetrievedObject const* robRef { tx->FindRetrievedObject(refObjId) };
			if (robRef && robRef->Changed())
				throw RetryTxException(m_txScheduler, robRef->m_touchedObject->m_commitStxId,
					"RetrieveObject: Reference object has been modified by another commit since it was read by the current transaction");
		}

		// Retrieve object
		if (objId == ObjId::None)
			return Rp<RcStr>();

		bool retrieveUncommitted {};
		TouchedObject* tob { FindTouchedObjectInMemory(objId) };
		if (tob)
		{
			// Object to be retrieved is being accessed by this or other transactions
			EnsureAbort(tob->m_committedState != ObjectState::Unknown);

			RetrievedObject const* retrievedObject { tx->FindRetrievedObject(objId) };
			if (retrievedObject)
			{
				// Object has been retrieved by this transaction
				if (retrievedObject->Changed())
					throw RetryTxException(m_txScheduler, retrievedObject->m_touchedObject->m_commitStxId,
						"RetrieveObject: Object has been modified by another commit since it was read by the current transaction");
			}
			else
			{
				// Object has NOT been retrieved by this transaction
				tx->AddRetrievedObject(tob);
				++(tob->m_refCount);
			}

			if (tob->m_uncommittedTxNr == tx->m_txNr)
			{
				// Object contains uncommitted changes by this transaction
				retrieveUncommitted = true;
			}
		}
		else
		{
			// Object not being accessed by any transaction
			AutoFree<TouchedObject> autoFreeTob { new TouchedObject(objId) };
			tob = autoFreeTob.Ptr();

			LoadTouchedObjectFromDisk(tob);
			InsertTouchedObject(autoFreeTob);

			if (tob->m_committedState == ObjectState::Removed)
			{
				// The object no longer exists
				return Rp<RcStr>();
			}

			tx->AddRetrievedObject(tob);
			++(tob->m_refCount);
		}

		if (retrieveUncommitted)
		{
			if (tob->m_uncommittedAction == ObjectAction::Remove)
				return Rp<RcStr>();
			else
				return tob->m_uncommittedData;
		}
		else
		{
			if (tob->m_committedState == ObjectState::Removed)
				return Rp<RcStr>();
			else
			{
				if (tob->m_committedState == ObjectState::ExistsButNotLoaded)
					LoadTouchedObjectFromDisk(tob);

				return tob->m_committedData;
			}
		}
	}


	// Replaces an existing object, keeping the same object ID.
	void ObjectStore::ReplaceObject(ObjId objId, ObjId refObjId, Rp<RcStr> data)
	{
		Locker locker { m_mx };
		if (m_tainted)
			throw Tainted();

		Tx* tx { (Tx*) TlsGetValue(m_tlsIndex) };
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);
		EnsureAbort(data.Any());

		// Validate reference object
		if (refObjId != ObjId::None)
		{
			EnsureThrow(refObjId != objId);
			RetrievedObject const* robRef { tx->FindRetrievedObject(refObjId) };
			if (robRef && robRef->Changed())
				throw RetryTxException(m_txScheduler, robRef->m_touchedObject->m_commitStxId,
					"ReplaceObject: Reference object has been modified by another commit since it was read by the current transaction");
		}

		// Replace object
		bool insertedBySameTx {};
		TouchedObject* tob { FindTouchedObjectInMemory(objId) };
		if (tob)
		{
			// Object is being accessed by this or other transactions
			EnsureAbort(tob->m_committedState != ObjectState::Unknown);

			RetrievedObject const* retrievedObject { tx->FindRetrievedObject(objId) };
			if (retrievedObject && retrievedObject->Changed())
			{
				// Prevent attempt to modify the object based on potentially outdated information
				throw RetryTxException(m_txScheduler, retrievedObject->m_touchedObject->m_commitStxId,
					"ReplaceObject: Object has been changed by another commit since it was read by the current transaction");
			}

			EnsureAbort(tob->m_committedState != ObjectState::Removed);		// Can't resurrect object; app didn't retrieve to check, otherwise it would fail with RetryTxException

			if (tob->m_uncommittedAction != ObjectAction::None)
				if (tob->m_uncommittedTxNr != tx->m_txNr)
				{
					// Only one modification can be in progress
					throw RetryTxException(m_txScheduler, tob->m_uncommittedStxId,
						"ReplaceObject: Object is already pending modification by another transaction");
				}
				else
				{
					EnsureAbort(tob->m_uncommittedAction != ObjectAction::Remove);
					if (tob->m_uncommittedAction == ObjectAction::Insert)
						insertedBySameTx = true;
				}
		}
		else
		{
			// Object not being accessed by any transaction
			AutoFree<TouchedObject> autoFreeTob { new TouchedObject(objId) };
			tob = autoFreeTob.Ptr();

			LoadTouchedObjectExistence(tob);
			InsertTouchedObject(autoFreeTob);

			if (tob->m_committedState == ObjectState::Removed)
				throw RetryTxException(m_txScheduler, tob->m_commitStxId,
					"ReplaceObject: Object has been removed by an already committed transaction");
		}

		ObjectAction::E prevUncommittedAction { tob->m_uncommittedAction };
		if (!insertedBySameTx)
			tob->m_uncommittedAction = ObjectAction::Replace;

		tob->m_uncommittedData  = data;
		tob->m_uncommittedTxNr  = tx->m_txNr;
		tob->m_uncommittedStxId = tx->m_stxId;

		if (tob->m_uncommittedAction != prevUncommittedAction)
		{
			tx->m_objectsToReplace.Add(tob);
			++(tob->m_refCount);
		}
	}



	void ObjectStore::RemoveObject(ObjId objId, ObjId refObjId)
	{
		Locker locker { m_mx };
		if (m_tainted)
			throw Tainted();

		Tx* tx { (Tx*) TlsGetValue(m_tlsIndex) };
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);
		EnsureAbort(objId != ObjId::Root);

		// Validate reference object
		if (refObjId != ObjId::None)
		{
			EnsureThrow(refObjId != objId);
			RetrievedObject const* robRef { tx->FindRetrievedObject(refObjId) };
			if (robRef && robRef->Changed())
				throw RetryTxException(m_txScheduler, robRef->m_touchedObject->m_commitStxId,
					"RemoveObject: Reference object has been modified by another commit since it was read by the current transaction");
		}

		// Remove object
		bool insertedBySameTx {};
		TouchedObject* tob { FindTouchedObjectInMemory(objId) };
		if (tob)
		{
			// Object is being accessed by this or other transactions
			EnsureAbort(tob->m_committedState != ObjectState::Unknown);

			if (tob->m_committedState == ObjectState::Removed)
				return;		// Object has already been removed by a committed transaction

			RetrievedObject const* retrievedObject { tx->FindRetrievedObject(objId) };
			if (retrievedObject && retrievedObject->Changed())
			{
				// Prevent attempt to remove the object based on potentially outdated information
				throw RetryTxException(m_txScheduler, retrievedObject->m_touchedObject->m_commitStxId,
					"RemoveObject: Object has been changed by another commit since it was read by the current transaction");
			}

			if (tob->m_uncommittedAction != ObjectAction::None &&
				tob->m_uncommittedTxNr != tx->m_txNr)
			{
				// Only one modification can be in progress
				throw RetryTxException(m_txScheduler, tob->m_uncommittedStxId,
					"RemoveObject: Object is already pending modification by another transaction");
			}

			if (tob->m_uncommittedAction == ObjectAction::Insert)
				insertedBySameTx = true;
		}
		else
		{
			// Object not being accessed by any transaction
			AutoFree<TouchedObject> autoFreeTob { new TouchedObject(objId) };
			tob = autoFreeTob.Ptr();

			LoadTouchedObjectExistence(tob);
			InsertTouchedObject(autoFreeTob);

			if (tob->m_committedState == ObjectState::Removed)
				return;		// Object has already been removed by a committed transaction
		}

		tob->m_uncommittedAction = ObjectAction::Remove;
		tob->m_uncommittedData.Clear();
		tob->m_uncommittedTxNr   = tx->m_txNr;
		tob->m_uncommittedStxId  = tx->m_stxId;

		if (!insertedBySameTx)
		{
			tx->m_objectsToRemove.Add(tob);
			++(tob->m_refCount);
		}
	}


	void ObjectStore::AddPostAbortAction(std::function<void()> action)
	{
		Locker locker(m_mx);
		if (m_tainted)
			throw Tainted();

		Tx* tx = (Tx*) TlsGetValue(m_tlsIndex);
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);

		tx->m_postAbortActions.push_back(action);
	}


	void ObjectStore::AddPostCommitAction(std::function<void()> action)
	{
		Locker locker(m_mx);
		if (m_tainted)
			throw Tainted();

		Tx* tx = (Tx*) TlsGetValue(m_tlsIndex);
		EnsureAbort(tx != nullptr);
		EnsureAbort(tx->m_state == Tx::InProgress);

		tx->m_postCommitActions.push_back(action);
	}



	// Private functions

	void ObjectStore::InsertTouchedObject(AutoFree<TouchedObject>& tob)
	{
		m_touchedObjects.Add(tob->m_objId, tob.Ptr());
		tob.Dismiss();
	}


	ObjectStore::TouchedObject* ObjectStore::FindTouchedObjectInMemory(ObjId objId)
	{
		TouchedObjectEntry* toe { m_touchedObjects.Find(objId) };
		if (toe)
			return toe->m_touchedObject;

		return nullptr;
	}


	void ObjectStore::LoadTouchedObjectExistence(TouchedObject* tob)
	{
		EnsureAbort(tob->m_committedState == ObjectState::Unknown);

		ObjId            objId      { tob->m_objId };
		StructuredOffset structured = GetStructuredOffset(objId.m_index * IndexEntryBytes);
		byte*            ifPage     { CachedReadPage(&m_indexFile, structured.pageOffsetInFile) };
		uint64*          indexEntry { (uint64*) (ifPage + structured.offsetInPage) };
		uint64           uniqueId   { indexEntry[0] };

		if (uniqueId == objId.m_uniqueId)
			tob->m_committedState = ObjectState::ExistsButNotLoaded;
		else
			tob->m_committedState = ObjectState::Removed;
	}


	void ObjectStore::LoadTouchedObjectFromDisk(TouchedObject* tob)
	{
		EnsureAbort(tob->m_committedState == ObjectState::Unknown ||
					tob->m_committedState == ObjectState::ExistsButNotLoaded);

		ObjId            objId          { tob->m_objId };
		StructuredOffset structured     = GetStructuredOffset(objId.m_index * IndexEntryBytes);
		byte*            page           { CachedReadPage(&m_indexFile, structured.pageOffsetInFile) };
		uint64*          indexEntry     { (uint64*) (page + structured.offsetInPage) };
		uint64           uniqueId       { indexEntry[0] };
		uint64           compactLocator { indexEntry[1] };

		if (uniqueId != objId.m_uniqueId)
		{
			// Object with requested unique ID is not in database
			tob->m_committedState = ObjectState::Removed;
			tob->m_committedData.Clear();
			tob->m_commitNr    = 0;
			tob->m_commitStxId = TxScheduler::InvalidStxId;
		}
		else
		{
			// Object is in database
			byte fileId = GetCompactLocatorFileId(compactLocator);
			uint64 fileOffset = GetCompactLocatorOffset(compactLocator);

			if (fileId == FileId::Oversize)
			{
				// Oversize file
				StorageFile* of = GetOversizeFile(tob->m_objId);
				uint32 objSize;
				of->ReadBytesUnaligned(&objSize, 4, 0);
			
				tob->m_committedData = new RcStr;
				tob->m_committedData->Resize(objSize);
				of->ReadBytesUnaligned(tob->m_committedData->Ptr(), objSize, 4);
			}
			else
			{
				// Regular storage file
				DataFile* df = FindDataFileById(fileId);
				EnsureAbort(df != nullptr);

				if (df->IsStoreCached())
				{
					structured = GetStructuredOffset(fileOffset);
					page = CachedReadPage(df, structured.pageOffsetInFile);
					byte* objDataStart = page + structured.offsetInPage;
					uint16 objSize = ((uint16*) objDataStart)[0];
					EnsureAbort(2 + ((sizet) objSize) >= df->ObjSizeMin());
					EnsureAbort(2 + ((sizet) objSize) <= df->ObjSizeMax());
					EnsureAbort(structured.offsetInPage + 2 + objSize <= Storage_PageSize);

					tob->m_committedData = new RcStr(objDataStart + 2, objSize);
				}
				else
				{
					uint32 objSize;
					df->ReadBytesUnaligned(&objSize, 4, fileOffset);
					EnsureAbort(4 + ((sizet) objSize) >= df->ObjSizeMin());
					EnsureAbort(4 + ((sizet) objSize) <= df->ObjSizeMax());

					tob->m_committedData = new RcStr;
					tob->m_committedData->Resize(objSize);
					df->ReadBytesUnaligned(tob->m_committedData->Ptr(), objSize, fileOffset + 4);
				}
			}

			tob->m_committedState = ObjectState::Loaded;
			tob->m_commitNr       = 0;
			tob->m_commitStxId    = TxScheduler::InvalidStxId;
		}
	}


	void ObjectStore::DecrementTouchedObjectRefCount(TouchedObject* tob)
	{
		if (--(tob->m_refCount) == 0)
			EnsureAbort(m_touchedObjects.Erase(tob->m_objId));
	}


	void ObjectStore::AddTouchedObjectToKeepAround(Tx* tx, TouchedObject* tob)
	{
		// Should be called for objects when they are committed, to ensure that the TouchedObject stays
		// around for at least as long as any earlier, still uncommitted transactions. These transactions
		// need to be able to check the TouchedObject's m_committedTxNr to prevent breaking serializability.

		for (Rp<Tx> const& otherTx : m_transactions)
			if (otherTx.Any())
				if (otherTx->m_txNr < tx->m_txNr)
				{
					tx->m_objectsToKeepAround.Add(tob);
					++(tob->m_refCount);
				}
	}


	ObjectStore::StructuredOffset ObjectStore::GetStructuredOffset(uint64 offsetInFile)
	{
		StructuredOffset structured;
		structured.pageOffsetInFile = ((offsetInFile / Storage_PageSize) * Storage_PageSize);
		structured.offsetInPage = offsetInFile - structured.pageOffsetInFile;

		return structured;
	}


	void ObjectStore::LoadMetaData()
	{
		Hash hash;
		hash.Create(CALG_SHA_256);
	
		AutoPage page { m_pageAllocator };
		if (m_metaFile.FileSize() == 0)
		{
			m_lastUniqueId = 0;
			m_lastWriteStateHash.Resize(hash.HashSize(), (byte) 0);
		}
		else
		{
			m_metaFile.ReadPages(page.Ptr(), 1, 0);
			m_lastUniqueId = ((uint64*) page.Ptr())[0];
		
			m_lastWriteStateHash.Resize(hash.HashSize());
			memcpy(m_lastWriteStateHash.Ptr(), page.Ptr() + 8, hash.HashSize());
		}

		m_lastWrittenUniqueId = m_lastUniqueId;
	}


	void ObjectStore::LoadFisPages()
	{
		EnsureAbort(!m_fisPages.Any());

		uint64 iffSize = GetWritePlanFileSize(&m_indexFreeFile);
		EnsureAbort((iffSize % Storage_PageSize) == 0);

		uint64 iffPages = iffSize / Storage_PageSize;
	
		uint64 pageOffset = 0;
		for (sizet iffPageIndex=0; iffPageIndex!=iffPages; ++iffPageIndex, pageOffset+=Storage_PageSize)
		{
			uint64* iffPage = (uint64*) CachedReadPage(&m_indexFreeFile, pageOffset);
			FisPage& fisPage = m_fisPages.Add();

			for (sizet i=0; i!=fisPage.m_state.Len(); ++i)
			{
				if (iffPage[i] == UINT64_MAX)
				{
					fisPage.m_state[i] = FreeIndexState::Invalid;
					++(fisPage.m_nrInvalid);
					++m_fisPages_totalNrInvalid;
				}
				else
				{
					fisPage.m_state[i] = FreeIndexState::Available;
					++(fisPage.m_nrAvailable);
				}
			}

			EnsureAbort(fisPage.m_nrAvailable + fisPage.m_nrInvalid + fisPage.m_nrReserved == fisPage.m_state.Len());
		}
	}


	uint64 ObjectStore::ReserveIndex()
	{
		uint64 iffSize = m_indexFreeFile.FileSize();
		EnsureAbort(iffSize == m_fisPages.Len() * Storage_PageSize);

		uint64 ifSize = m_indexFile.FileSize();
		EnsureAbort((ifSize % Storage_PageSize) == 0);

		// Try to find an available index in free list file
		for (sizet fisPageIndex=0; fisPageIndex!=m_fisPages.Len(); ++fisPageIndex)
		{
			FisPage& fisPage = m_fisPages[fisPageIndex];
			if (fisPage.m_nrAvailable > 0)
			{
				sizet i = 0;
				while (true)
				{
					if (fisPage.m_state[i] == FreeIndexState::Available)
					{
						uint64 iffOffset = iffSize - ((m_fisPages.Len() - fisPageIndex) * Storage_PageSize);
						uint64* iffPage = (uint64*) CachedReadPage(&m_indexFreeFile, iffOffset);
						fisPage.m_state[i] = FreeIndexState::Reserved;
						++(fisPage.m_nrReserved);
						--(fisPage.m_nrAvailable);
						return iffPage[i];
					}

					// Must be able to find an available entry, given that m_nrAvailable != 0
					++i;
					EnsureAbort(i != fisPage.m_state.Len());
				}
			}
		}

		// No indices left in free list file. Expand the free list file with a new page of indices obtained from expanding the index file
		uint64* iffPage = (uint64*) CachedReadPage(&m_indexFreeFile, iffSize);
		for (sizet i=0; i!=UInt64PerPage; ++i)
			iffPage[i] = (ifSize / IndexEntryBytes) + i;

		RunWritePlan([&] {
				Locator iffLocator(&m_indexFreeFile, iffSize);
				AddWritePlanEntry_CachedPage(iffLocator, iffPage);

				uint64* indexPages[IndexPagesPerFreeFilePage];
				for (sizet i=0; i!=IndexPagesPerFreeFilePage; ++i)
				{
					uint64 pageOffset = ifSize + (i * Storage_PageSize);
					indexPages[i] = (uint64*) CachedReadPage(&m_indexFile, pageOffset);

					Locator ifLocator(&m_indexFile, pageOffset);
					AddWritePlanEntry_CachedPage(ifLocator, indexPages[i]);
				}

				// Must add meta file write entry, so that the stored last write hash gets updated
				byte* mfPage = CachedReadPage(&m_metaFile, 0);
				Locator mfLocator(&m_metaFile, 0);
				AddWritePlanEntry_CachedPage(mfLocator, mfPage);
			});

		FisPage& fisPage = m_fisPages.Add();
		memset(fisPage.m_state.Ptr(), FreeIndexState::Available, fisPage.m_state.Len());
		fisPage.m_state[0] = FreeIndexState::Reserved;
		fisPage.m_nrAvailable = fisPage.m_state.Len() - 1;
		fisPage.m_nrReserved = 1;

		return iffPage[0];
	}


	void ObjectStore::ConsolidateFreeIndexState()
	{
		if ((m_fisPages_totalNrInvalid / UInt64PerPage) > (m_fisPages.Len() / 2))
		{
			// We will at least halve the number of pages. Consolidate free index state
			sizet firstInvalidFisPageIndex = SIZE_MAX;
			for (sizet i=0; i!=m_fisPages.Len(); ++i)
				if (m_fisPages[i].m_nrInvalid > 0)
				{
					firstInvalidFisPageIndex = i;
					break;
				}

			EnsureAbort(firstInvalidFisPageIndex != SIZE_MAX);

			sizet fisReadPageIndex = firstInvalidFisPageIndex;
			sizet fisWritePageIndex = fisReadPageIndex;

			FisPage* fisReadPage = &(m_fisPages[fisReadPageIndex]);
			FisPage* fisWritePage = &(m_fisPages[fisWritePageIndex]);

			uint64 iffReadPageOffset = firstInvalidFisPageIndex * Storage_PageSize;
			uint64 iffWritePageOffset = iffReadPageOffset;
		
			uint64* iffReadPage = (uint64*) CachedReadPage(&m_indexFreeFile, iffReadPageOffset);
			uint64* iffWritePage = iffReadPage;

			sizet writeIndex = 0;
		
			EnsureAbort(m_fisPages_totalNrInvalid >= fisWritePage->m_nrInvalid);
			m_fisPages_totalNrInvalid -= fisWritePage->m_nrInvalid;

			fisWritePage->m_nrAvailable = 0;
			fisWritePage->m_nrInvalid = 0;
			fisWritePage->m_nrReserved = 0;

			while (true)
			{
				for (sizet readIndex=0; readIndex!=fisReadPage->m_state.Len(); ++readIndex)
					if (fisReadPage->m_state[readIndex] != FreeIndexState::Invalid)
					{
						fisWritePage->m_state[writeIndex] = fisReadPage->m_state[readIndex];
						iffWritePage[writeIndex] = iffReadPage[readIndex];

						switch (fisWritePage->m_state[writeIndex])
						{
						case FreeIndexState::Available: ++(fisWritePage->m_nrAvailable); break; 
						case FreeIndexState::Invalid:   ++(fisWritePage->m_nrInvalid);   ++m_fisPages_totalNrInvalid; break;
						case FreeIndexState::Reserved:  ++(fisWritePage->m_nrReserved);  break;
						default: EnsureAbort(!"Invalid free index state");
						}

						if (++writeIndex == fisWritePage->m_state.Len())
						{
							EnsureAbort(fisWritePage->m_nrAvailable + fisWritePage->m_nrInvalid + fisWritePage->m_nrReserved == fisWritePage->m_state.Len());
							AddWritePlanEntry_CachedPage(Locator(&m_indexFreeFile, iffWritePageOffset), iffWritePage);

							++fisWritePageIndex;
							fisWritePage = &(m_fisPages[fisWritePageIndex]);

							EnsureAbort(m_fisPages_totalNrInvalid >= fisWritePage->m_nrInvalid);
							m_fisPages_totalNrInvalid -= fisWritePage->m_nrInvalid;

							fisWritePage->m_nrAvailable = 0;
							fisWritePage->m_nrInvalid = 0;
							fisWritePage->m_nrReserved = 0;

							iffWritePageOffset += Storage_PageSize;
							iffWritePage = (uint64*) CachedReadPage(&m_indexFreeFile, iffWritePageOffset);

							writeIndex = 0;
						}
					}

				if (++fisReadPageIndex == m_fisPages.Len())
					break;

				fisReadPage = &(m_fisPages[fisReadPageIndex]);

				iffReadPageOffset += Storage_PageSize;
				iffReadPage = (uint64*) CachedReadPage(&m_indexFreeFile, iffReadPageOffset);
			}

			for (; writeIndex != fisWritePage->m_state.Len(); ++writeIndex)
			{
				fisWritePage->m_state[writeIndex] = FreeIndexState::Invalid;
				iffWritePage[writeIndex] = UINT64_MAX;

				++(fisWritePage->m_nrInvalid);
				++m_fisPages_totalNrInvalid;
			}

			EnsureAbort(fisWritePage->m_nrAvailable + fisWritePage->m_nrInvalid + fisWritePage->m_nrReserved == fisWritePage->m_state.Len());
			AddWritePlanEntry_CachedPage(Locator(&m_indexFreeFile, iffWritePageOffset), iffWritePage, WritePlanEntry::WriteEof);

			++fisWritePageIndex;
			while (fisWritePageIndex < m_fisPages.Len())
				m_fisPages.PopLast();

			EnsureAbort(iffWritePageOffset + Storage_PageSize == m_fisPages.Len() * Storage_PageSize);
		}
	}


	bool ObjectStore::FindDataFileByObjectSizeNoLen(sizet n, DataFile*& df, FreeFile*& ff)
	{
		for (sizet i=0; i!=NrDataFiles; ++i)
		{
			sizet lenLen = m_dataFiles[i].OneOrMoreEntriesPerPage() ? 2U : 4U;
			sizet nWithLen = n + lenLen;
			if (nWithLen >= m_dataFiles[i].ObjSizeMin() && nWithLen <= m_dataFiles[i].ObjSizeMax())
			{
				df = &m_dataFiles[i];
				ff = &m_dataFreeFiles[i];
				return true;
			}
		}

		return false;
	}


	uint64 ObjectStore::GetWritePlanFileSize(StorageFile* sf)
	{
		EnsureAbort(sf->Id() < m_writePlanFileSizes.Len());
		uint64 size = m_writePlanFileSizes[sf->Id()];
		EnsureAbort((size % Storage_PageSize) == 0);
		EnsureAbort(size != UINT64_MAX);
		return size;
	}


	StorageFile* ObjectStore::GetOversizeFile(ObjId oversizeFileId)
	{
		StorageFile* sf = m_openOversizeFiles.FindOrInsertEntry(oversizeFileId);

		if (!sf->IsOpen())
		{
			Str filePath = GetOversizeFilePath(oversizeFileId);

			sf->SetId(FileId::Oversize);
			sf->SetFullPath(filePath);
			sf->Open();
		}

		return sf;
	}


	void ObjectStore::DeleteOversizeFile(ObjId oversizeFileId)
	{
		m_openOversizeFiles.RemoveEntries(oversizeFileId, oversizeFileId);

		Str filePath = GetOversizeFilePath(oversizeFileId);
		if (!DeleteFileW(WinStr(filePath).Z()))
		{
			DWORD rc = GetLastError();
			if (m_completingWritePlan && rc == ERROR_FILE_NOT_FOUND)
			{
				// The file was most likely removed during previous attempt to execute this write plan.
			}
			else
				throw WinErr<>(rc, Str("ObjectStore::DeleteOversizeFile: Error deleting '").Add(filePath));
		}
	}


	Str ObjectStore::GetOversizeDirPath()
	{
		return JoinPath(m_dir, "Oversize");
	}


	Str ObjectStore::GetOversizeFilePath(ObjId oversizeFileId)
	{
		Str fileName = Str().UInt(oversizeFileId.m_index).Add("-").UInt(oversizeFileId.m_uniqueId).Add(".dat");
		return JoinPath(GetOversizeDirPath(), fileName);
	}


	byte* ObjectStore::CachedReadPage(StorageFile* sf, uint64 offset)
	{
		EnsureAbort(sf->IsStoreCached());

		// Attempt to find page in cache
		Locator locator;
		locator.Set(sf, offset);

		CachedPage* cachedPage = m_cachedPages.FindOrInsertEntry(locator);
		if (!cachedPage->Inited())
		{
			cachedPage->Init(m_pageAllocator);
			if (offset < GetWritePlanFileSize(sf))
				sf->ReadPages(cachedPage->m_page, 1, offset);
			else
			{
				// The offset requested is beyond EOF according to our current write plan.
				// Leave page set to zeroes. 
			}
		}

		return cachedPage->m_page;
	}


	void ObjectStore::PruneCache()
	{
		m_openOversizeFiles.PruneEntries(m_openOversizeFilesTarget);
		m_cachedPages.PruneEntries(m_cachedPagesTarget);
	}


	StorageFile* ObjectStore::FindStorageFileById(byte fileId)
	{
			 if (fileId == FileId::Meta)	  return &m_metaFile;
		else if (fileId == FileId::Index)	  return &m_indexFile;
		else if (fileId == FileId::IndexFree) return &m_indexFreeFile;
	
		StorageFile* sf = FindDataFileById(fileId);
		if (!sf)
			sf = FindDataFreeFileById(fileId);
		return sf;
	}


	DataFile* ObjectStore::FindDataFileById(byte fileId)
	{
		if (fileId >= FileId::DataStart && fileId < FileId::DataStart + NrDataFiles)
			return &m_dataFiles[fileId - FileId::DataStart];

		return 0;
	}


	FreeFile* ObjectStore::FindDataFreeFileById(byte fileId)
	{
		if (fileId >= FileId::DataFreeStart && fileId < FileId::DataFreeStart + NrDataFiles)
			return &m_dataFreeFiles[fileId - FileId::DataFreeStart];

		return 0;
	}


	void ObjectStore::RunWritePlan(std::function<void()> f)
	{
		StartWritePlan();

		// If an exception occurs during write plan preparation and execution, the cache can no longer be trusted.
		// In that case, the database can no longer be used, and must be re-initialized.
		try
		{
			f();
			RecordAndExecuteWritePlan();
		}
		catch (...)
		{
			m_tainted = true;
			throw;
		}

		ClearWritePlan();
	}


	void ObjectStore::StartWritePlan()
	{
		EnsureAbort(m_writePlanState == WritePlanState::None);
		m_writePlanState = WritePlanState::Started;
	}


	void ObjectStore::ClearWritePlan()
	{
		EnsureAbort(m_writePlanState == WritePlanState::Executing);

		for (WritePlanEntry*& entry : m_writePlanEntries)
		{
			delete entry;
			entry = nullptr;
		}

		m_writePlanEntries.Clear();
		m_replaceableWritePlanEntries.clear();
		m_writePlanState = WritePlanState::None;
	}


	void ObjectStore::AddWritePlanEntry(AutoFree<WritePlanEntry>& autoFreeEntry)
	{
		EnsureAbort(m_writePlanState == WritePlanState::Started);

		WritePlanEntry* entry = autoFreeEntry.Ptr();
		EnsureAbort((entry->m_locator.m_offset % Storage_PageSize) == 0);

		byte const fileId = entry->m_locator.m_fileId;
		if (fileId == FileId::Oversize)
		{
			// Writes to oversize files can be optimized before this function and don't have to be additionally optimized here
			// Add new write plan entry
			entry->m_entryIndex = m_writePlanEntries.Len();
			m_writePlanEntries.Add();
			m_writePlanEntries.Last() = autoFreeEntry.Dismiss();
		}
		else
		{
			// Not an oversize file
			EnsureAbort(fileId <= FileId::MaxNonSpecial);
			EnsureAbort(entry->m_type == WritePlanEntry::Write || entry->m_type == WritePlanEntry::WriteEof);

			// Either the file is store-cached and the write plan has one page exactly; or the file is OS-cached and the write plan has multiple pages
			StorageFile const* const storageFile = FindStorageFileById(fileId);
			bool const storeCached = storageFile->IsStoreCached();
			if (storeCached)
				EnsureAbort(entry->m_nrPages == 1);
			else
				EnsureAbort(entry->m_nrPages > 1);

			// See what happens to file size after this entry
			uint64 prevFileSize = m_writePlanFileSizes[fileId];
			uint64 newFileSize = prevFileSize;
			uint64 writeSize = entry->m_nrPages * Storage_PageSize;
			uint64 afterWriteOffset = entry->m_locator.m_offset + writeSize;

			if (prevFileSize < afterWriteOffset || entry->m_type == WritePlanEntry::WriteEof)
				newFileSize = afterWriteOffset;

			if (newFileSize == prevFileSize)
				entry->m_type = WritePlanEntry::Write;		// Entry does not change file size. If type is WriteEof, change to normal Write

			if (!storeCached)
			{
				// Will file size change?
				if (newFileSize != prevFileSize)
					m_writePlanFileSizes[fileId] = newFileSize;

				// OS-cached, non-oversize files are those that contain multiple pages per entry
				// Writes to multi-page files can be optimized before this function and don't have to be additionally optimized here
				// Add new write plan entry
				entry->m_entryIndex = m_writePlanEntries.Len();
				m_writePlanEntries.Add();
				m_writePlanEntries.Last() = autoFreeEntry.Dismiss();
			}
			else
			{
				// File is store-cached
				bool replaceable = true;

				// Will file size change?
				if (newFileSize != prevFileSize)
				{
					if (newFileSize > prevFileSize)
					{
						// We only support enlarging store-cached files by one page at a time.
						// If we were to support multiple-page enlargements, we would need to
						// update non-cached pages in between to make sure they're zeroed out.
						EnsureAbort(newFileSize - prevFileSize == Storage_PageSize);
					}
					else
					{
						// We are making the file smaller
						// Remove any cached pages containing content from the same file past the new end of file

						Locator locatorRemoveStart = entry->m_locator;
						locatorRemoveStart.m_offset = newFileSize;

						Locator locatorRemoveEnd = entry->m_locator;
						locatorRemoveEnd.m_offset = UINT64_MAX;

						m_cachedPages.RemoveEntries(locatorRemoveStart, locatorRemoveEnd);
					}

					m_writePlanFileSizes[fileId] = newFileSize;

					// Entries that change file size must be executed in order, so they're not added to the replaceable list.
					replaceable = false;
				}

				// Add new write plan entry
				entry->m_entryIndex = m_writePlanEntries.Len();
				m_writePlanEntries.Add();
				m_writePlanEntries.Last() = autoFreeEntry.Dismiss();

				// Is there an existing replaceable write plan entry for the same file and offset?
				WritePlanEntriesByLocator::iterator locatorIt = m_replaceableWritePlanEntries.find(entry->m_locator);
				if (locatorIt == m_replaceableWritePlanEntries.end())
				{
					// There's no earlier replaceable write plan entry for this file and offset
					if (replaceable)
						m_replaceableWritePlanEntries.insert(std::make_pair(entry->m_locator, entry));
				}
				else
				{
					// Remove previous write plan entry
					sizet prevEntryIndex = locatorIt->second->m_entryIndex;
					delete m_writePlanEntries[prevEntryIndex];
					m_writePlanEntries[prevEntryIndex] = nullptr;

					// Update replaceable locator entry (if replaceable) or remove it (if not)
					if (replaceable)
						locatorIt->second = entry;
					else
						m_replaceableWritePlanEntries.erase(locatorIt);
				}

				if (entry->m_type == WritePlanEntry::WriteEof)
				{
					// Entry sets EOF
					// Remove any previous write plan entries that reference the same file at a higher offset
					Locator locatorRemoveStart = entry->m_locator;
					locatorRemoveStart.m_offset = locatorRemoveStart.m_offset + 1;

					Locator locatorRemoveEnd = entry->m_locator;
					locatorRemoveEnd.m_offset = UINT64_MAX;

					WritePlanEntriesByLocator::iterator itRemoveStart = m_replaceableWritePlanEntries.lower_bound(locatorRemoveStart);
					WritePlanEntriesByLocator::iterator itRemoveEnd = m_replaceableWritePlanEntries.upper_bound(locatorRemoveEnd);
		
					if (itRemoveStart != itRemoveEnd)
					{
						WritePlanEntriesByLocator::iterator itRemove = itRemoveStart;
						while (itRemove != itRemoveEnd)
						{
							sizet entryIndex = itRemove->second->m_entryIndex;
							delete m_writePlanEntries[entryIndex];
							m_writePlanEntries[entryIndex] = nullptr;
							++itRemove;
						}
			
						m_replaceableWritePlanEntries.erase(itRemoveStart, itRemoveEnd);
					}

					// Remove any cached pages containing content from the same file at a higher offset
					m_cachedPages.RemoveEntries(locatorRemoveStart, locatorRemoveEnd);
				}
			}
		}
	}


	void ObjectStore::RecordAndExecuteWritePlan()
	{
		EnsureAbort(m_writePlanState == WritePlanState::Started);

		m_writePlanState = WritePlanState::Executing;

		// Encode the write plan
		Hash hash;
		hash.Create(CALG_SHA_256);

		sizet hashedSize = 8 + hash.HashSize();
		for (WritePlanEntry* entry : m_writePlanEntries)
			if (entry)
				if (entry->m_type == WritePlanEntry::DeleteOversizeFile)
					hashedSize += WritePlanEntry::SerializedBytes_DeleteOversizedFile;
				else if (entry->m_nrPages == 1)
					hashedSize += WritePlanEntry::SerializedBytes_WriteSinglePage;
				else
					hashedSize += WritePlanEntry::SerializedBytes_WriteMultiPageHeader + (entry->m_nrPages * Storage_PageSize);

		sizet const encodedSizeWithLen = hashedSize + hash.HashSize();
		SequentialPages encodedPlanPages { m_pageAllocator, encodedSizeWithLen };
		WritePlanEncoder encoder { encodedPlanPages.Ptr(), encodedSizeWithLen };

		uint64 const encodedSizeNoLen = encodedSizeWithLen - 8;
		encoder.Encode(&encodedSizeNoLen, 8);
		encoder.Encode(m_lastWriteStateHash);

		for (WritePlanEntry* entry : m_writePlanEntries)
			if (entry)
			{
				byte entryTypeAndFlags = (byte) entry->m_type;
				if (entry->m_type != WritePlanEntry::DeleteOversizeFile && entry->m_nrPages > 1)
					entryTypeAndFlags |= WritePlanEntry::Flag_MultiPage;

				encoder.Encode(&entry->m_locator.m_fileId, 1);
				encoder.Encode(&entryTypeAndFlags, 1);
				encoder.Encode(&entry->m_locator.m_oversizeFileId.m_index, 8);
				encoder.Encode(&entry->m_locator.m_oversizeFileId.m_uniqueId, 8);
				encoder.Encode(&entry->m_locator.m_offset, 8);

				if (entry->m_type != WritePlanEntry::DeleteOversizeFile)
				{
					if (entry->m_nrPages > 1)
					{
						EnsureAbort(entry->m_nrPages <= UINT32_MAX);
						uint32 nrPages = (uint32) entry->m_nrPages;
						encoder.Encode(&nrPages, 4);
					}

					encoder.Encode(entry->m_firstPage, entry->m_nrPages * Storage_PageSize);
				}
			}

		m_lastWriteStateHash.Clear();
		hash.Process(Seq(encodedPlanPages.Ptr(), hashedSize)).Final(m_lastWriteStateHash);
		encoder.Encode(m_lastWriteStateHash);
		EnsureAbort(encoder.Remaining() == 0);

		// Record the write plan in journal file
		m_journalFile.WritePages(encodedPlanPages.Ptr(), encodedPlanPages.Nr(), 0);

		// Perform write actions
		PerformWritePlanActions();

		// Clear journal file
		m_journalFile.Clear();
	}


	void ObjectStore::PerformWritePlanActions()
	{
		EnsureAbort(m_writePlanState == WritePlanState::Executing);

		bool lastWriteIsMeta = false;
		for (WritePlanEntry* entry : m_writePlanEntries)
			if (entry)
			{
				// What kind of file are we working on?
				StorageFile* sf = nullptr;
				if (entry->m_locator.m_fileId != FileId::Oversize)
				{
					// Regular storage file
					sf = FindStorageFileById(entry->m_locator.m_fileId);
					EnsureAbort(sf != nullptr);
				}
				else
				{
					// Oversize file
					if (entry->m_type == WritePlanEntry::DeleteOversizeFile)
						DeleteOversizeFile(entry->m_locator.m_oversizeFileId);
					else
						sf = GetOversizeFile(entry->m_locator.m_oversizeFileId);
				}

				if (entry->m_type != WritePlanEntry::DeleteOversizeFile)
				{
					EnsureAbort(entry->m_type == WritePlanEntry::Write || entry->m_type == WritePlanEntry::WriteEof);

					// If we're writing to beginning of meta file, update the write state hash we're going to store.
					// The write state hash is either calculated in RecordAndExecuteWritePlan(), or obtained in CompleteWritePlanFromJournal(),
					// so it is not yet available at the time the write plan is constructed.
					if (entry->m_locator.m_fileId == FileId::Meta && entry->m_locator.m_offset == 0)
					{
						Mem::Copy(entry->m_firstPage + 8, m_lastWriteStateHash.Ptr(), m_lastWriteStateHash.Len());
						lastWriteIsMeta = true;
					}
					else
						lastWriteIsMeta = false;

					// If we're performing a write plan test, induce a chance of failure
					if (m_writePlanTest && !m_completingWritePlan)
					{
						uint64 v;
						Crypt::GenRandom(&v, sizeof(v));
						if ((v % m_writeFailOdds) == 0)
							throw ZLitErr("Simulated write failure");
					}

					// Write to file
					sf->WritePages(entry->m_firstPage, entry->m_nrPages, entry->m_locator.m_offset);

					if (entry->m_type == WritePlanEntry::WriteEof)
					{
						sizet writeSize = entry->m_nrPages * Storage_PageSize;
						uint64 newFileSize = entry->m_locator.m_offset + writeSize;
						sf->SetEof(newFileSize);
					}
				}
			}

		EnsureAbort(lastWriteIsMeta);
	}


	bool ObjectStore::CompleteWritePlanFromJournal()
	{
		m_completingWritePlan = true;
		OnExit toggleCompletingFlag([&] () { m_completingWritePlan = false; });

		// Read write plan from journal
		AutoPage journalPage { m_pageAllocator };
		m_journalFile.ReadPages(journalPage.Ptr(), 1, 0);

		uint64 encodedSize64;
		memcpy(&encodedSize64, journalPage.Ptr(), 8);

		EnsureAbort(encodedSize64 <= SIZE_MAX);
		sizet encodedSize = (sizet) encodedSize64;

		// Is there a write plan in the journal?
		bool writePlanExecuted = false;
		if (encodedSize != 0)
		{
			// Check if there is enough data in journal to decode write plan.
			// If there isn't, assume writing to journal file failed, and clear it.
			sizet totalSize = 8 + encodedSize;
			if (m_journalFile.FileSize() < totalSize)
			{
				if (m_writePlanTest)
					EnsureAbort(!"Journal file size inconsistent with content. This must not happen during write plan testing.");
			}
			else
			{
				SequentialPages encodedPlanPages { m_pageAllocator, totalSize };
				m_journalFile.ReadPages(encodedPlanPages.Ptr(), encodedPlanPages.Nr(), 0);

				// Calculate digest
				Hash hash;
				hash.Create(CALG_SHA_256);

				EnsureAbort(encodedSize >= 2 * hash.HashSize());
				sizet totalHashedSize = totalSize - hash.HashSize();
				hash.Process(Seq(encodedPlanPages.Ptr(), totalHashedSize));

				Str calculatedDigest;
				hash.Final(calculatedDigest);

				// Verify hash. If it checks out, we execute the write plan stored in the journal file.
				// If hash doesn't check out, we assume writing to journal file failed, and clear it.
				Seq storedDigest { encodedPlanPages.Ptr() + totalHashedSize, hash.HashSize() };	
				if (calculatedDigest != storedDigest)
				{
					if (m_writePlanTest)
						EnsureAbort(!"Journal digest inconsistent with stored digest. This must not happen during write plan testing.");
				}
				else
				{
					// The write state hash before completing this write plan has to match either:
					// - the preceding stored digest this write plan is based on
					// - the stored digest of this write plan
					if (m_lastWriteStateHash != storedDigest)
					{
						Seq prevStoredDigest { encodedPlanPages.Ptr() + 8, hash.HashSize() };
						EnsureAbort(m_lastWriteStateHash == prevStoredDigest);
					}

					m_lastWriteStateHash = storedDigest;

					// Deserialize write plan
					Seq reader { encodedPlanPages.Ptr(), totalHashedSize };
					reader.DropBytes(8 + hash.HashSize());

					StartWritePlan();

					while (reader.Any())
					{
						// Deserialize entry
						Locator locator;
						byte entryTypeAndFlags;

						EnsureAbort(reader.ReadBytesInto(&locator.m_fileId, 1));
						EnsureAbort(reader.ReadBytesInto(&entryTypeAndFlags, 1));
						EnsureAbort(reader.ReadBytesInto(&locator.m_oversizeFileId.m_index, 8));
						EnsureAbort(reader.ReadBytesInto(&locator.m_oversizeFileId.m_uniqueId, 8));
						EnsureAbort(reader.ReadBytesInto(&locator.m_offset, 8));

						WritePlanEntry::Type entryType = (WritePlanEntry::Type) (entryTypeAndFlags & WritePlanEntry::EntryTypeMask);
						bool multiPage = ((entryTypeAndFlags & WritePlanEntry::Flag_MultiPage) != 0);

						uint32 nrPages = 1;
						if (multiPage)
							EnsureAbort(reader.ReadBytesInto(&nrPages, 4));

						if (entryType == WritePlanEntry::DeleteOversizeFile)
						{
							AddWritePlanEntry_DeleteOversizeFile(locator);
						}
						else
						{
							if (locator.m_fileId == FileId::Oversize)
							{
								// Oversize file write
								sizet writeSize = ((sizet) nrPages) * Storage_PageSize;
								Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, writeSize);
								EnsureAbort(reader.ReadBytesInto(writePages.Ptr(), writeSize));
								AddWritePlanEntry_SequentialPages(locator, writePages, entryType);
							}
							else
							{
								// Regular file write
								StorageFile* sf = FindStorageFileById(locator.m_fileId);
								EnsureAbort(sf != nullptr);

								if (sf->IsStoreCached())
								{
									EnsureAbort(nrPages == 1);
									byte* writePage = CachedReadPage(sf, locator.m_offset);
									EnsureAbort(reader.ReadBytesInto(writePage, Storage_PageSize));
									AddWritePlanEntry_CachedPage(locator, writePage, entryType);
								}
								else
								{
									EnsureAbort(nrPages > 1);
									sizet writeSize = ((sizet) nrPages) * Storage_PageSize;
									Rp<Rc<SequentialPages>> writePages = new Rc<SequentialPages>(m_pageAllocator, writeSize);
									EnsureAbort(reader.ReadBytesInto(writePages.Ptr(), writeSize));
									AddWritePlanEntry_SequentialPages(locator, writePages, entryType);
								}
							}
						}
					}

					// Perform write actions
					m_writePlanState = WritePlanState::Executing;
					PerformWritePlanActions();
					ClearWritePlan();

					writePlanExecuted = true;
				}
			}
		}

		// Clear journal file
		m_journalFile.Clear();
	
		return writePlanExecuted;
	}

}
