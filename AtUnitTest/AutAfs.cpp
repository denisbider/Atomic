#include "AutIncludes.h"

#include <random>


char const* const c_zAfsTestSubDirName = "AfsTest";



class AfsTest : public Afs
{
public:
	AfsTest(Seq testName, AfsStorage& storage, CaseMatch cm)
		: m_testName(testName)
	{
		Console::Out(Str::Join("\r\n", testName, "\r\n"));
		SetStorage(storage);
		if (cm == CaseMatch::Exact)
			SetNameComparer(CompareExact);
		else
			SetNameComparer(CompareInsensitive);
	}

	void DispMaxNameBytes()
	{
		Str msg;
		msg.Add("MaxNameBytes: ").UInt(MaxNameBytes()).Add("\r\n");
		Console::Out(msg);
	}

	uint64 PerfTest(uint64 durationMs, std::function<bool()> testIteration)
	{
		ULONGLONG startTicks = GetTickCount64();
		ULONGLONG ticksElapsed;

		while (true)
		{
			bool keepGoing = testIteration();
			ticksElapsed = GetTickCount64() - startTicks;
			if (!keepGoing || ticksElapsed >= durationMs)
				return ticksElapsed;
		}
	}

	~AfsTest()
	{
		if (!std::uncaught_exception())
			Console::Out(Str::Join(m_testName, " OK\r\n"));
	}

private:
	Str m_testName;

	static int CompareInsensitive (Seq a, Seq b) { return a.Compare(b, CaseMatch::Insensitive ); }
	static int CompareExact       (Seq a, Seq b) { return a.Compare(b, CaseMatch::Exact       ); }
};



struct AfsTestMem
{
	AfsTestMem(Seq testName, uint32 blockSize, uint64 maxNrBlocks, CaseMatch cm)
		: m_storage(blockSize, maxNrBlocks)
		, m_afs(testName, m_storage, cm) {}

	AfsMemStorage m_storage;
	AfsTest       m_afs;
};



enum class DeleteExisting { No, Yes };

struct AfsTestFileStorage : AfsFileStorage
{
	void TestInit(Seq testName, uint32 blockSize, uint64 maxSizeBytes, DeleteExisting deleteExisting, AfsFileStorage::Consistency consistency)
	{
		if (!File::Exists_IsDirectory(c_zAfsTestSubDirName))
			if (!CreateDirectoryW(WinStr(c_zAfsTestSubDirName).Z(), nullptr))
				{ LastWinErr e; throw e.Make("CreateDirectoryW"); }

		m_dataFilePath = JoinPath(c_zAfsTestSubDirName, testName);
		m_dataFilePath.Add(".dat");

		m_journalFilePath = AfsFileStorage::GetJournalFilePath(m_dataFilePath);

		if (DeleteExisting::Yes == deleteExisting)
			DeleteFiles();

		Init(m_dataFilePath, blockSize, consistency);
		SetMaxSizeBytes(maxSizeBytes);
	}

	void DeleteFiles()
	{
		if (File::Exists_NotDirectory(m_dataFilePath))
			if (!DeleteFileW(WinStr(m_dataFilePath).Z()))
				{ LastWinErr e; throw e.Make("DeleteFileW for data file"); }

		if (File::Exists_NotDirectory(m_journalFilePath))
			if (!DeleteFileW(WinStr(m_journalFilePath).Z()))
				{ LastWinErr e; throw e.Make("DeleteFileW for journal file"); }
	}

	Str m_dataFilePath;
	Str m_journalFilePath;
};



struct AfsTestFile
{
	AfsTestFile(Seq testName, uint32 blockSize, uint64 maxSizeBytes, DeleteExisting deleteExisting, AfsFileStorage::Consistency consistency, CaseMatch cm)
		: m_afs(testName, m_storage, cm)
	{
		m_storage.TestInit(testName, blockSize, maxSizeBytes, deleteExisting, consistency);
	}

	AfsTestFileStorage m_storage;
	AfsTest            m_afs;
};



class AfsTestRandom
{
public:
	AfsTestRandom(Seq testName, AfsStorage& storage, uint32 seed, sizet nrActions, bool verbose)
		: m_nrActions(nrActions)
		, m_mt(seed)
		, m_afs(testName, storage, CaseMatch::Exact)
		, m_verbose(verbose)
	{
		m_writeBuf.ResizeExact(MaxReadWriteBytes);
	}

	void Run()
	{
		Dir* rootDir {};

		{
			Time now = ++m_now;
			Rp<Dir> dir = new Dir;
			dir->m_id = ObjId::Root;
			dir->m_name = "/";
			dir->m_metaData = GenMetaData();
			dir->m_createTime = now;
			dir->m_modifyTime = now;

			AfsResult::E r = m_afs.Init(dir->m_metaData, now);
			EnsureThrowWithNr(AfsResult::OK == r, r);
			rootDir = dir.Ptr();
			m_dirs.Add(std::move(dir));
		}

		m_maxNameBytes = m_afs.MaxNameBytes();

		uint64 const startFreeSpaceBlocks = m_afs.FreeSpaceBlocks();
		uint64 const startFreeSpaceBytes  = m_afs.FreeSpaceBytes();
		sizet const progressThreshold = m_nrActions / 50;
		
		for (sizet i=1; i<=m_nrActions; ++i)
		{
			if (!m_verbose)
				if (0 == (i % progressThreshold))
					Console::Out(".");

			uint32 action = GenBits(8);
			     if (action <=   5) Action_DirCreate();
			else if (action <=  25) Action_FileCreate();
			else if (action <=  30) Action_FileCreateDeleteMany();
			else if (action <=  40) Action_DirStat();
			else if (action <=  60) Action_FileStat();
			else if (action <=  70) Action_DirSetStat();
			else if (action <=  90) Action_FileSetStat();
			else if (action <= 100) Action_DirMove();
			else if (action <= 120) Action_FileMove();
			else if (action <= 130) Action_DirRename();
			else if (action <= 150) Action_FileRename();
			else if (action <= 160) Action_DirRead();
			else if (action <= 195) Action_FileWrite();
			else if (action <= 200) Action_FileWriteMany();
			else if (action <= 205) Action_FileSetSize();
			else if (action <= 250) Action_FileRead();
			else if (action <= 254) Action_FileDelete();
			else if (action <= 255) Action_DirDeleteRecursive();
			else EnsureThrow(!"Unexpected action value");
		}

		SubAction_DirDeleteRecursive(*rootDir);

		uint64 const endFreeSpaceBlocks = m_afs.FreeSpaceBlocks();
		uint64 const endFreeSpaceBytes  = m_afs.FreeSpaceBytes();
		m_afs.VerifyFreeList();
		EnsureThrowWithNr2(startFreeSpaceBlocks == endFreeSpaceBlocks, startFreeSpaceBlocks, endFreeSpaceBlocks );
		EnsureThrowWithNr2(startFreeSpaceBytes  == endFreeSpaceBytes,  startFreeSpaceBytes,  endFreeSpaceBytes  );

		Str msg;
		if (!m_verbose) msg.Add("\r\n");
		msg.Add("DirCreate: "       ).UInt(m_nrDirCreate)
		   .Add(", FileCreate: "    ).UInt(m_nrFileCreate)
		   .Add(", DirsPeak: "      ).UInt(m_nrDirsPeak)
		   .Add(", FilesPeak: "     ).UInt(m_nrFilesPeak)
		   .Add(", DirEntriesPeak: ").UInt(m_dirEntriesPeak)
		   .Add(", FileLenPeak: "   ).UInt(m_fileLenPeak)
		   .Add(", DirStat: "       ).UInt(m_nrDirStat)
		   .Add(", FileStat: "      ).UInt(m_nrFileStat)
		   .Add(", DirSetStat: "    ).UInt(m_nrDirSetStat)
		   .Add(", FileSetStat: "   ).UInt(m_nrFileSetStat)
		   .Add(", DirMove: "       ).UInt(m_nrDirMove)
		   .Add(", FileMove: "      ).UInt(m_nrFileMove)
		   .Add(", DirRename: "     ).UInt(m_nrDirRename)
		   .Add(", FileRename: "    ).UInt(m_nrFileRename)
		   .Add(", DirRead: "       ).UInt(m_nrDirRead)
		   .Add(", BytesWritten: "  ).UInt(m_nrBytesWritten)
		   .Add(", BytesRead: "     ).UInt(m_nrBytesRead)
		   .Add(", FileDelete: "    ).UInt(m_nrFileDelete)
		   .Add(", DirDelete: "     ).UInt(m_nrDirDelete)
		   .Add(", OutOfSpace: "    ).UInt(m_nrOutOfSpace)
		   .Add("\r\n");
		Console::Out(msg);
	}

private:
	enum { MetaDataBytes = 17 };
	sizet const  m_nrActions;
	bool const   m_verbose;

	std::mt19937 m_mt;
	Time         m_now;
	uint32       m_bits;
	uint32       m_nrBitsAvail {};
	AfsTest      m_afs;
	uint32       m_maxNameBytes {};


	struct File;
	struct Dir;

	struct DirEntryByNameLen : Afs::DirEntry
	{
		File* m_file {};
		Dir*  m_dir  {};
		
		Seq Key() const { return m_name; }
	};

	struct DirEntryByNameLenCrit : MapKey<KeyInVal<DirEntryByNameLen>>
	{
		// We sort names longest names first, then alphabetically, to ensure that recursive delete
		// removes files with the longest names first. This avoids an out of space condition that is possible
		// if deleting entries with shorter names before those with longer names.
		static bool KeyEarlier(KeyOrRef a, KeyOrRef b) { return (a.n > b.n) || (a.n == b.n && a < b); }
		static bool KeyEqual  (KeyOrRef a, KeyOrRef b) { return a == b; }
	};

	using DirEntries = MapCore<DirEntryByNameLenCrit>;

	struct Dir : RefCountable
	{
		Dir*       m_parentDir {};		// nullptr for root dir
		Str        m_name;				// For display purposes, set to "/" for root dir
		ObjId      m_id;
		Str        m_metaData;
		DirEntries m_entries;
		Time       m_createTime;
		Time       m_modifyTime;
		sizet      m_indexAdded {};

		ObjId ParentDirId() const { return (nullptr != m_parentDir) ? m_parentDir->m_id : ObjId::None; }
	};

	struct File : RefCountable
	{
		Dir*  m_parentDir {};
		Str   m_name;
		ObjId m_id;
		Str   m_metaData;
		Str   m_content;
		Time  m_createTime;
		Time  m_modifyTime;
		sizet m_indexAdded {};
	};

	RpVec<Dir>  m_dirs;
	RpVec<File> m_files;
	uint64      m_nrDirCreate    { 1 };
	uint64      m_nrFileCreate   {};
	sizet       m_nrDirsPeak     { 1 };
	sizet       m_nrFilesPeak    {};
	uint64      m_dirEntriesPeak {};
	uint64      m_fileLenPeak    {};
	uint64      m_nrDirStat      {};
	uint64      m_nrFileStat     {};
	uint64      m_nrDirSetStat   {};
	uint64      m_nrFileSetStat  {};
	uint64      m_nrDirMove      {};
	uint64      m_nrFileMove     {};
	uint64      m_nrDirRename    {};
	uint64      m_nrFileRename   {};
	uint64      m_nrDirRead      {};
	uint64      m_nrBytesWritten {};
	uint64      m_nrBytesRead    {};
	uint64      m_nrFileDelete   {};
	uint64      m_nrDirDelete    {};
	uint64      m_nrOutOfSpace   {};
	Str         m_writeBuf;


	Dir& GetRandomDir()
	{
		sizet dirIndex = m_mt() % m_dirs.Len();
		return m_dirs[dirIndex].Ref();
	}


	File& GetRandomFile()
	{
		EnsureThrow(m_files.Any());
		sizet fileIndex = m_mt() % m_files.Len();
		return m_files[fileIndex].Ref();
	}


	sizet FindDirIndex(Dir* dir)
	{
		EnsureThrow(m_dirs.Any());
		sizet i = dir->m_indexAdded;
		if (i >= m_dirs.Len())
			i = m_dirs.Len() - 1;

		while (true)
		{
			if (m_dirs[i].Ptr() == dir)
				return i;

			EnsureThrow(0 != i);
			--i;
		}
	}


	sizet FindFileIndex(File* file)
	{
		EnsureThrow(m_files.Any());
		sizet i = file->m_indexAdded;
		if (i >= m_files.Len())
			i = m_files.Len() - 1;

		while (true)
		{
			if (m_files[i].Ptr() == file)
				return i;

			EnsureThrow(0 != i);
			--i;
		}
	}


	void Action_DirCreate()
	{
		Dir& parentDir = GetRandomDir();
		SubAction_DirCreate(parentDir);
		SubAction_DirRead(parentDir);
	}


	Dir* SubAction_DirCreate(Dir& parentDir)
	{
		Time now = ++m_now;
		DirEntryByNameLen entry;
		entry.m_name = GenName();
		entry.m_type = Afs::ObjType::Dir;

		AfsResult::E expectResult = AfsResult::OK;
		DirEntries::It it = parentDir.m_entries.Find(entry.m_name);
		if (it.Any())
			expectResult = AfsResult::NameExists;

		Str metaData = GenMetaData();
		AfsResult::E r = m_afs.DirCreate(parentDir.m_id, entry.m_name, metaData, now, entry.m_id);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" DirCreate ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(" ").Add(entry.m_name)
				.Add(": ").Add(AfsResult::Name(r)).Add(" ").Obj(entry.m_id).Add("\r\n"));

		Dir* dirPtr {};
		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				Rp<Dir> dir = new Dir;
				dir->m_id         = entry.m_id;
				dir->m_parentDir  = &parentDir;
				dir->m_name       = entry.m_name;
				dir->m_metaData   = std::move(metaData);
				dir->m_createTime = now;
				dir->m_modifyTime = now;

				entry.m_dir = dir.Ptr();
				parentDir.m_entries.Add(std::move(entry));
				parentDir.m_modifyTime = now;
				dir->m_indexAdded = m_dirs.Len();
				dirPtr = dir.Ptr();
				m_dirs.Add(std::move(dir));

				++m_nrDirCreate;
				if (m_nrDirsPeak < m_dirs.Len())
					m_nrDirsPeak = m_dirs.Len();
				if (m_dirEntriesPeak < parentDir.m_entries.Len())
					m_dirEntriesPeak = parentDir.m_entries.Len();
			}
		}

		return dirPtr;
	}


	void Action_FileCreate()
	{
		Dir& parentDir = GetRandomDir();
		SubAction_FileCreate(parentDir);
		SubAction_DirRead(parentDir);
	}


	File* SubAction_FileCreate(Dir& parentDir)
	{
		Time now = ++m_now;
		DirEntryByNameLen entry;
		entry.m_name = GenName();
		entry.m_type = Afs::ObjType::File;

		AfsResult::E expectResult = AfsResult::OK;
		DirEntries::It it = parentDir.m_entries.Find(entry.m_name);
		if (it.Any())
			expectResult = AfsResult::NameExists;

		Str metaData = GenMetaData();
		AfsResult::E r = m_afs.FileCreate(parentDir.m_id, entry.m_name, metaData, now, entry.m_id);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileCreate ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(", ").Add(entry.m_name)
				.Add(": ").Add(AfsResult::Name(r)).Add(" ").Obj(entry.m_id).Add("\r\n"));

		File* filePtr {};
		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				Rp<File> file = new File;
				file->m_id = entry.m_id;
				file->m_parentDir  = &parentDir;
				file->m_name       = entry.m_name;
				file->m_metaData   = std::move(metaData);
				file->m_createTime = now;
				file->m_modifyTime = now;

				entry.m_file = file.Ptr();
				parentDir.m_entries.Add(std::move(entry));
				parentDir.m_modifyTime = now;
				file->m_indexAdded = m_files.Len();
				filePtr = file.Ptr();
				m_files.Add(std::move(file));

				++m_nrFileCreate;
				if (m_nrFilesPeak < m_files.Len())
					m_nrFilesPeak = m_files.Len();
				if (m_dirEntriesPeak < parentDir.m_entries.Len())
					m_dirEntriesPeak = parentDir.m_entries.Len();
			}
		}

		return filePtr;
	}


	void Action_FileCreateDeleteMany()
	{
		Dir& parentDir = GetRandomDir();
		Dir* dir = SubAction_DirCreate(parentDir);
		if (nullptr == dir)
			return;

		sizet nrCreate = 1000 + GenBits(13);
		Vec<File*> files;
		files.ReserveExact(nrCreate);

		for (sizet i=0; i!=nrCreate; ++i)
		{
			File* file = SubAction_FileCreate(*dir);
			if (nullptr == file)
				break;

			files.Add(file);
		}

		SubAction_DirRead(*dir);

		sizet const nrSkip = 1 + GenBits(4);
		sizet const nrDelete = 1 + GenBits(4);
		bool skipping = ((GenBits(1) % 1) == 0);
		sizet counter {};

		while (files.Any())
			for (sizet i=0; i!=files.Len(); )
			{
				if (!counter--)
				{
					skipping = !skipping;
					counter = skipping ? nrSkip : nrDelete;
				}

				if (skipping)
					++i;
				else
				{
					File* file = files[i];
					sizet fileIndex = FindFileIndex(file);
					if (SubAction_FileDelete(fileIndex))
						files.Erase(i, 1);
				}
			}

		SubAction_DirRead(*dir);
		SubAction_DirDeleteRecursive(*dir);
	}


	void Action_DirStat()
	{
		Time now = ++m_now;
		Dir& dir = GetRandomDir();

		Afs::StatInfo info;
		AfsResult::E r = m_afs.ObjStat(dir.m_id, info);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" DirStat ").Obj(dir.m_id).Add(" ").Add(dir.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));
		EnsureThrowWithNr(AfsResult::OK == r, r);

		EnsureThrow(info.m_type          == Afs::ObjType::Dir     );
		EnsureThrow(info.m_id            == dir.m_id              );
		EnsureThrow(info.m_parentId      == dir.ParentDirId()     );
		EnsureThrow(info.m_dir_nrEntries == dir.m_entries.Len()   );
		EnsureThrow(info.m_createTime    == dir.m_createTime      );
		EnsureThrow(info.m_modifyTime    == dir.m_modifyTime      );
		EnsureThrow(info.m_metaData      == dir.m_metaData        );
		++m_nrDirStat;
	}


	void Action_FileStat()
	{
		if (!m_files.Any()) return;

		Time now = ++m_now;
		File& file = GetRandomFile();

		Afs::StatInfo info;
		AfsResult::E r = m_afs.ObjStat(file.m_id, info);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileStat ").Obj(file.m_id).Add(" ").Add(file.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));
		EnsureThrowWithNr(AfsResult::OK == r, r);

		EnsureThrow(info.m_type           == Afs::ObjType::File     );
		EnsureThrow(info.m_id             == file.m_id              );
		EnsureThrow(info.m_parentId       == file.m_parentDir->m_id );
		EnsureThrow(info.m_file_sizeBytes == file.m_content.Len()   );
		EnsureThrow(info.m_createTime     == file.m_createTime      );
		EnsureThrow(info.m_modifyTime     == file.m_modifyTime      );
		EnsureThrow(info.m_metaData       == file.m_metaData        );
		++m_nrFileStat;
	}


	void Action_DirSetStat()
	{
		Time now = ++m_now;
		Dir& dir = GetRandomDir();
		Afs::StatInfo info;
		uint32 statFields = GenBits(3);
		if (0 != (statFields & Afs::StatField::CreateTime )) { dir.m_createTime = now;         info.m_createTime = dir.m_createTime; }
		if (0 != (statFields & Afs::StatField::ModifyTime )) { dir.m_modifyTime = now;         info.m_modifyTime = dir.m_modifyTime; }
		if (0 != (statFields & Afs::StatField::MetaData   )) { dir.m_metaData = GenMetaData(); info.m_metaData   = dir.m_metaData;   }
		AfsResult::E r = m_afs.ObjSetStat(dir.m_id, info, statFields);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" DirSetStat ").Obj(dir.m_id).Add(" ").Add(dir.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));
		EnsureThrow(AfsResult::OK == r);
		++m_nrDirSetStat;
	}


	void Action_FileSetStat()
	{
		if (!m_files.Any()) return;

		Time now = ++m_now;
		File& file = GetRandomFile();

		Afs::StatInfo info;
		uint32 statFields = GenBits(3);
		if (0 != (statFields & Afs::StatField::CreateTime )) { file.m_createTime = now;         info.m_createTime = file.m_createTime; }
		if (0 != (statFields & Afs::StatField::ModifyTime )) { file.m_modifyTime = now;         info.m_modifyTime = file.m_modifyTime; }
		if (0 != (statFields & Afs::StatField::MetaData   )) { file.m_metaData = GenMetaData(); info.m_metaData   = file.m_metaData;   }

		AfsResult::E r = m_afs.ObjSetStat(file.m_id, info, statFields);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileSetStat ").Obj(file.m_id).Add(" ").Add(file.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		EnsureThrow(AfsResult::OK == r);
		++m_nrFileSetStat;
	}


	void Action_DirMove()
	{
		// Attempt to find a directory that's not the root directory. There might not be one
		Time now = ++m_now;
		Dir& dir = GetRandomDir();
		if (!dir.m_parentDir)
			return;

		// Select a new parent directory. Make sure it's a different directory
		Dir& parentDir = *dir.m_parentDir;
		Dir& newParentDir = GetRandomDir();
		if (newParentDir.m_id == parentDir.m_id)
			return;

		// Check if moving to this directory would create a circular reference
		AfsResult::E expectResult = AfsResult::OK;
		Dir* ancestorDir = &newParentDir;
		do
		{
			if (ancestorDir->m_id == dir.m_id)
			{
				expectResult = AfsResult::MoveDestInvalid;
				break;
			}
			ancestorDir = ancestorDir->m_parentDir;
		}
		while (ancestorDir);

		if (AfsResult::OK == expectResult)
		{
			DirEntries::It it = newParentDir.m_entries.Find(dir.m_name);
			if (it.Any())
				expectResult = AfsResult::NameExists;
		}

		AfsResult::E r = m_afs.ObjMove(parentDir.m_id, dir.m_name, newParentDir.m_id, dir.m_name, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" DirMove ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(", ").Obj(dir.m_id).Add(" ").Add(dir.m_name)
				.Add(", ").Obj(newParentDir.m_id).Add(" ").Add(newParentDir.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				DirEntries::It it = parentDir.m_entries.Find(dir.m_name);
				EnsureThrow(it.Any());
				newParentDir.m_entries.Add(it.Ref());
				newParentDir.m_modifyTime = now;
				parentDir.m_entries.Erase(it);
				parentDir.m_modifyTime = now;
				dir.m_parentDir = &newParentDir;
				++m_nrDirMove;
			}
		}

		SubAction_DirRead(parentDir);
		SubAction_DirRead(newParentDir);
	}


	void Action_FileMove()
	{
		if (!m_files.Any()) return;

		Time now = ++m_now;
		File& file = GetRandomFile();
		Dir& parentDir = *file.m_parentDir;
		Dir& newParentDir = GetRandomDir();
		if (newParentDir.m_id == parentDir.m_id)
			return;

		AfsResult::E expectResult = AfsResult::OK;
		DirEntries::It it = newParentDir.m_entries.Find(file.m_name);
		if (it.Any())
			expectResult = AfsResult::NameExists;

		AfsResult::E r = m_afs.ObjMove(parentDir.m_id, file.m_name, newParentDir.m_id, file.m_name, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileMove ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(", ").Obj(file.m_id).Add(" ").Add(file.m_name)
				.Add(", ").Obj(newParentDir.m_id).Add(" ").Add(newParentDir.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				it = parentDir.m_entries.Find(file.m_name);
				EnsureThrow(it.Any());
				newParentDir.m_entries.Add(it.Ref());
				newParentDir.m_modifyTime = now;
				parentDir.m_entries.Erase(it);
				parentDir.m_modifyTime = now;
				file.m_parentDir = &newParentDir;
				++m_nrFileMove;
			}
		}

		SubAction_DirRead(parentDir);
		SubAction_DirRead(newParentDir);
	}


	void Action_DirRename()
	{
		Time now = ++m_now;
		Dir& dir = GetRandomDir();
		if (!dir.m_parentDir)
			return;

		Dir& parentDir = *dir.m_parentDir;
		Str newName = GenName();

		AfsResult::E expectResult = AfsResult::OK;
		DirEntries::It it = parentDir.m_entries.Find(newName);
		if (it.Any())
			expectResult = AfsResult::NameExists;

		AfsResult::E r = m_afs.ObjMove(parentDir.m_id, dir.m_name, parentDir.m_id, newName, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" DirRename ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(", ").Obj(dir.m_id).Add(" ").Add(dir.m_name)
				.Add(", ").Add(newName).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				it = parentDir.m_entries.Find(dir.m_name);
				EnsureThrow(it.Any());
				DirEntryByNameLen e = it.Ref();
				e.m_name = newName;
				parentDir.m_entries.Erase(it);
				parentDir.m_entries.Add(std::move(e));
				parentDir.m_modifyTime = now;
				dir.m_name = newName;
				++m_nrDirRename;
			}
		}

		SubAction_DirRead(parentDir);
	}


	void Action_FileRename()
	{
		if (!m_files.Any()) return;

		Time now = ++m_now;
		File& file = GetRandomFile();
		Dir& parentDir = *file.m_parentDir;
		Str newName = GenName();

		AfsResult::E expectResult = AfsResult::OK;
		DirEntries::It it = parentDir.m_entries.Find(newName);
		if (it.Any())
			expectResult = AfsResult::NameExists;

		AfsResult::E r = m_afs.ObjMove(parentDir.m_id, file.m_name, parentDir.m_id, newName, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileRename ").Obj(parentDir.m_id).Add(" ").Add(parentDir.m_name).Add(", ").Obj(file.m_id).Add(" ").Add(file.m_name)
				.Add(", ").Add(newName).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
			++m_nrOutOfSpace;
		else
		{
			EnsureThrowWithNr2(expectResult == r, expectResult, r);
			if (AfsResult::OK == r)
			{
				it = parentDir.m_entries.Find(file.m_name);
				EnsureThrow(it.Any());
				DirEntryByNameLen e = it.Ref();
				e.m_name = newName;
				parentDir.m_entries.Erase(it);
				parentDir.m_entries.Add(std::move(e));
				parentDir.m_modifyTime = now;
				file.m_name = newName;
				++m_nrFileRename;
			}
		}

		SubAction_DirRead(parentDir);
	}


	void Action_DirRead()
	{
		Dir& dir = GetRandomDir();
		SubAction_DirRead(dir);
	}


	void SubAction_DirRead(Dir& dir)
	{
		Time now = ++m_now;
		Str lastNameRead;

		bool reachedEnd {};
		Vec<Afs::DirEntry> entries;
		sizet nrMatched {};

		while (true)
		{
			entries.Clear();
			AfsResult::E r = m_afs.DirRead(dir.m_id, lastNameRead, entries, reachedEnd);
			if (m_verbose)
				Console::Out(Str().UInt(now.ToFt()).Add(" DirRead ").Obj(dir.m_id).Add(" ").Add(dir.m_name).Add(", '").Add(lastNameRead)
					.Add("': ").Add(AfsResult::Name(r)).Add(" ").UInt(entries.Len()).Add(reachedEnd ? " END" : " more").Add("\r\n"));

			EnsureThrowWithNr(AfsResult::OK == r, r);

			for (Afs::DirEntry const& e : entries)
			{
				DirEntries::It it = dir.m_entries.Find(e.m_name);
				EnsureThrow(it.Any());
				EnsureThrow(e.m_id   == it->m_id   );
				EnsureThrow(e.m_name == it->m_name );
				EnsureThrow(e.m_type == it->m_type );
				++nrMatched;
			}

			if (reachedEnd)
				break;

			lastNameRead = std::move(entries.Last().m_name);
		}

		EnsureThrowWithNr2(nrMatched == dir.m_entries.Len(), nrMatched, dir.m_entries.Len());
		++m_nrDirRead;
	}


	void Action_FileWrite()
	{
		if (!m_files.Any()) return;
		File& file = GetRandomFile();
		SubAction_FileWrite(file);
	}


	uint32 SubAction_FileWrite(File& file)
	{
		Time now = ++m_now;
		sizet offset = GenWriteOffset(file.m_content.Len());
		uint32 writeLen = GenReadWriteLen();

		EnsureThrowWithNr2(m_writeBuf.Len() >= writeLen, m_writeBuf.Len(), writeLen);
		int fillByte = (int) GenBits(8);
		memset(m_writeBuf.Ptr(), fillByte, writeLen);

		Seq writeData { m_writeBuf.Ptr(), writeLen };
		AfsResult::E r = m_afs.FileWrite(file.m_id, offset, writeData, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileWrite ").Obj(file.m_id).Add(" ").Add(file.m_name).Add(", ").UInt(offset)
				.Add(" ").UInt(writeData.n).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
		{
			++m_nrOutOfSpace;
			return 0;
		}

		EnsureThrowWithNr(AfsResult::OK == r, r);
		sizet sizeNeeded = offset + writeLen;
		if (file.m_content.Len() < sizeNeeded)
		{
			file.m_content.ResizeAtLeast(sizeNeeded);
			if (m_fileLenPeak < sizeNeeded)
				m_fileLenPeak = sizeNeeded;
		}

		memcpy(file.m_content.Ptr() + offset, writeData.p, writeData.n);
		file.m_modifyTime = now;
		m_nrBytesWritten += writeLen;
		return writeLen;
	}


	void Action_FileWriteMany()
	{
		if (!m_files.Any()) return;

		File& file = GetRandomFile();
		sizet nrWrites = 100 + GenBits(9);
		bool outOfSpace {};

		for (sizet i=0; i!=nrWrites; ++i)
			if (0 == SubAction_FileWrite(file))
			{
				outOfSpace = true;
				break;
			}

		SubAction_FileRead(file, 0, file.m_content.Len());

		if (outOfSpace)
		{
			sizet fileIndex = FindFileIndex(&file);
			SubAction_FileDelete(fileIndex);
		}
	}


	void Action_FileSetSize()
	{
		if (!m_files.Any()) return;

		Time now = ++m_now;
		File& file = GetRandomFile();
		sizet newSize = GenSetSize(file.m_content.Len());

		uint64 actualNewSize {};
		AfsResult::E r = m_afs.FileSetSize(file.m_id, newSize, actualNewSize, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileSetSize ").Obj(file.m_id).Add(" ").Add(file.m_name).Add(", ").UInt(newSize)
				.Add(": ").Add(AfsResult::Name(r)).Add(" ").UInt(actualNewSize).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
		{
			++m_nrOutOfSpace;

			if (actualNewSize != file.m_content.Len())
			{
				EnsureThrowWithNr2(actualNewSize > file.m_content.Len(), actualNewSize, file.m_content.Len());
				EnsureThrowWithNr2(actualNewSize < newSize, actualNewSize, newSize);
				file.m_content.ResizeAtLeast((sizet) actualNewSize);
				file.m_modifyTime = now;
			}
		}
		else
		{
			EnsureThrowWithNr(AfsResult::OK == r, r);
			EnsureThrowWithNr2(newSize == actualNewSize, newSize, actualNewSize);
			file.m_content.ResizeAtLeast((sizet) newSize);
			file.m_modifyTime = now;
		}
	}


	void Action_FileRead()
	{
		if (!m_files.Any()) return;

		File& file = GetRandomFile();
		sizet readLen = GenReadWriteLen();

		sizet const maxOffsetEnd = SatSub<sizet>(file.m_content.Len(), readLen / 2);
		sizet offset {};
		if (maxOffsetEnd)
			offset = m_mt() % maxOffsetEnd;

		SubAction_FileRead(file, offset, readLen);
	}


	void SubAction_FileRead(File& file, sizet offset, sizet readLen)
	{
		Time now = ++m_now;
		Seq reader = file.m_content;
		reader.DropBytes(offset);
		reader = reader.ReadBytes(readLen);

		sizet bytesRead {};
		bool haveReachedEnd {};
		AfsResult::E r = m_afs.FileRead(file.m_id, offset, readLen, [&] (Seq data, bool reachedEnd)
			{
				EnsureThrow(!haveReachedEnd);
				EnsureThrow(reader.StripPrefixExact(data));
				bytesRead += data.n;
				if (reachedEnd)
					haveReachedEnd = true;
			} );

		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileRead ").Obj(file.m_id).Add(" ").Add(file.m_name)
				.Add(", ").UInt(offset).Add(" ").UInt(readLen).Add(": ")
				.Add(AfsResult::Name(r)).Add(" ").UInt(bytesRead).Add(haveReachedEnd ? " EOF" : " more").Add("\r\n"));

		EnsureThrow(AfsResult::OK == r);
		EnsureThrowWithNr(!reader.n, reader.n);
		EnsureThrowWithNr2(bytesRead <= readLen, bytesRead, readLen);
		if (bytesRead < readLen)
			EnsureThrow(offset + bytesRead == file.m_content.Len());
		else
			EnsureThrow(offset + bytesRead <= file.m_content.Len());

		m_nrBytesRead += bytesRead;
	}


	void Action_FileDelete()
	{
		if (!m_files.Any()) return;

		sizet fileIndex = m_mt() % m_files.Len();
		Dir* dir = m_files[fileIndex]->m_parentDir;
		SubAction_FileDelete(fileIndex);
		SubAction_DirRead(*dir);
	}


	bool SubAction_FileDelete(sizet fileIndex)
	{
		Time now = ++m_now;
		File& file = m_files[fileIndex].Ref();
		Dir& dir = *file.m_parentDir;

		DirEntries::It it = dir.m_entries.Find(file.m_name);
		EnsureThrow(it.Any());

		AfsResult::E r = m_afs.ObjDelete(dir.m_id, file.m_name, now);
		if (m_verbose)
			Console::Out(Str().UInt(now.ToFt()).Add(" FileDelete ").Obj(dir.m_id).Add(" ").Add(dir.m_name)
				.Add(", ").Obj(file.m_id).Add(" ").Add(file.m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

		if (AfsResult::OutOfSpace == r)
		{
			++m_nrOutOfSpace;
			return false;
		}

		EnsureThrow(AfsResult::OK == r);
		dir.m_modifyTime = now;
		dir.m_entries.Erase(it);
		m_files.Erase(fileIndex, 1);
		++m_nrFileDelete;
		return true;
	}


	void Action_DirDeleteRecursive()
	{
		Dir& dir = GetRandomDir();
		SubAction_DirDeleteRecursive(dir);
	}


	void SubAction_DirDeleteRecursive(Dir& dir)
	{
		Time now = ++m_now;
		Dir* parentDir = dir.m_parentDir;
		Str dirName = dir.m_name;

		struct DelEntry
		{
			Dir*           m_dir {};
			DirEntries::It m_it;

			DelEntry(Dir* dir) : m_dir(dir), m_it(dir->m_entries.begin()) {}
		};

		Vec<DelEntry> delEntries;
		delEntries.Add(&dir);

		bool anyRemoved {};
		while (delEntries.Any())
		{
			DelEntry& e = delEntries.Last();
			if (e.m_it == e.m_dir->m_entries.end())
			{
				if (e.m_dir->m_id == ObjId::Root)
				{
					if (anyRemoved)
						e.m_dir->m_modifyTime = now;
				}
				else
				{
					sizet dirIndex = FindDirIndex(e.m_dir);

					AfsResult::E r = m_afs.ObjDelete(e.m_dir->m_parentDir->m_id, e.m_dir->m_name, now);
					if (m_verbose)
						Console::Out(Str().UInt(now.ToFt()).Add(" DirDelete dir ").Obj(e.m_dir->m_parentDir->m_id).Add(" ").Add(e.m_dir->m_parentDir->m_name)
							.Add(", ").Obj(e.m_dir->m_id).Add(" ").Add(e.m_dir->m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

					EnsureThrow(AfsResult::OK == r);
					m_dirs.Erase(dirIndex, 1);
					anyRemoved = true;
				}
				delEntries.PopLast();
			}
			else
			{
				if (Afs::ObjType::File == e.m_it->m_type)
				{
					EnsureThrow(nullptr != e.m_it->m_file);
					sizet fileIndex = FindFileIndex(e.m_it->m_file);

					AfsResult::E r = m_afs.ObjDelete(e.m_dir->m_id, e.m_it->m_name, now);
					if (m_verbose)
						Console::Out(Str().UInt(now.ToFt()).Add(" DirDelete file ").Obj(e.m_dir->m_id).Add(" ").Add(e.m_dir->m_name)
							.Add(", ").Obj(e.m_it->m_id).Add(" ").Add(e.m_it->m_name).Add(": ").Add(AfsResult::Name(r)).Add("\r\n"));

					EnsureThrow(AfsResult::OK == r);
					m_files.Erase(fileIndex, 1);
					e.m_it = e.m_dir->m_entries.Erase(e.m_it);
					anyRemoved = true;
				}
				else
				{
					EnsureThrow(nullptr != e.m_it->m_dir);
					Dir* dirToAdd = e.m_it->m_dir;
					e.m_it = e.m_dir->m_entries.Erase(e.m_it);
					delEntries.Add(dirToAdd);
				}
			}
		}

		++m_nrDirDelete;

		if (parentDir)
		{
			DirEntries::It it = parentDir->m_entries.Find(dirName);
			EnsureThrow(it.Any());
			parentDir->m_entries.Erase(it);
			parentDir->m_modifyTime = now;

			SubAction_DirRead(*parentDir);
		}
	}


	Str GenStr(sizet n)
	{
		static char const* const c_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

		Str s;
		s.ReserveExact(n);
		while (n--)
			s.Ch(c_alphabet[GenBits(6)]);
		return s;
	}


	Str GenMetaData() { return GenStr(MetaDataBytes); }


	Str GenName()
	{
		uint32 nameLenCat = GenBits(2);
		sizet nameLen = 1;
		     if (0 == nameLenCat) nameLen += m_mt() %      (m_maxNameBytes / 4);
		else if (1 == nameLenCat) nameLen += m_mt() %      (m_maxNameBytes / 2);
		else if (2 == nameLenCat) nameLen += m_mt() % (3 * (m_maxNameBytes / 4));
		else if (3 == nameLenCat) nameLen += m_mt() %       m_maxNameBytes;
		else EnsureThrow(!"Unexpected nameLenCat value");

		return GenStr(nameLen);
	}


	uint32 GenBits(uint32 nrBits)
	{
		if (0 == nrBits) return 0;
		if (32 == nrBits) return m_mt();

		EnsureThrow(0 != nrBits);
		EnsureThrowWithNr(nrBits < 32, nrBits);

		if (nrBits > m_nrBitsAvail)
		{
			m_bits = m_mt();
			m_nrBitsAvail = 32;
		}

		uint32 v = (m_bits & ((1U << nrBits) - 1));
		m_bits >>= nrBits;
		m_nrBitsAvail -= nrBits;
		return v;
	}


	sizet GenWriteOffset(sizet maxOffset)
	{
		if (!maxOffset)
			return 0;

		uint32 v = GenBits(1);
		EnsureThrowWithNr(v <= 1, v);
		if (0 == v)
			return maxOffset;

		return m_mt() % maxOffset;
	}


	sizet GenSetSize(sizet curSize)
	{
		if (!curSize)
			return 0;

		uint32 v = GenBits(2);
		EnsureThrowWithNr(v <= 3, v);
		if (0 == v) return 0;
		if (1 == v) return m_mt() % (1 + ((3 * curSize) / 4));
		if (2 == v) return m_mt() % curSize;
		return m_mt() % ((5 * curSize) / 4);
	}


	enum { MaxReadWriteBytes = 65536 };

	uint32 GenReadWriteLen()
	{
		uint32 v = GenBits(4);
		EnsureThrowWithNr(v <= 15, v);
		return 1 + GenBits(v + 1);
	}
};



struct AfsTestMemRandom
{
public:
	AfsTestMemRandom(uint32 seed, uint32 blockSize, uint64 maxNrBlocks, sizet nrActions, bool verbose)
		: m_testName(Str("MemRandom-").UInt(seed).Ch('-').UInt(blockSize).Ch('-').UInt(maxNrBlocks).Ch('-').UInt(nrActions))
		, m_storage(blockSize, maxNrBlocks)
		, m_testRandom(m_testName, m_storage, seed, nrActions, verbose) {}

	void Run()
	{
		m_testRandom.Run();

		Str msg;
		msg.Add("AllocatorCacheHits: "     ).UInt(m_storage.Allocator().NrCacheHits())
		   .Add(", AllocatorCacheMisses: " ).UInt(m_storage.Allocator().NrCacheMisses())
		   .Add("\r\n");
		Console::Out(msg);
	}

	Str           m_testName;
	AfsMemStorage m_storage;
	AfsTestRandom m_testRandom;
};



struct AfsTestFileRandom
{
public:
	AfsTestFileRandom(uint32 seed, uint32 blockSize, uint64 maxSizeBytes, sizet nrActions, AfsFileStorage::Consistency consistency, bool verbose)
		: m_testName(Str("FileRandom-").UInt(seed).Ch('-').UInt(blockSize).Ch('-').UInt(maxSizeBytes).Ch('-').UInt(nrActions))
		, m_testRandom(m_testName, m_storage, seed, nrActions, verbose)
	{
		m_storage.TestInit(m_testName, blockSize, maxSizeBytes, DeleteExisting::Yes, consistency);
		if (AfsFileStorage::Consistency::VerifyJournal == consistency)
			Console::Out("Verifying journal\r\n");
	}

	void Run()
	{
		m_testRandom.Run();

		Str msg;
		msg.Add("BlockCacheHits: "         ).UInt(m_storage.NrCacheHits())
		   .Add(", BlockCacheMisses: "     ).UInt(m_storage.NrCacheMisses())
		   .Add(", AllocatorCacheHits: "   ).UInt(m_storage.Allocator().NrCacheHits())
		   .Add(", AllocatorCacheMisses: " ).UInt(m_storage.Allocator().NrCacheMisses())
		   .Add("\r\n");
		Console::Out(msg);
	}

	Str                m_testName;
	AfsTestFileStorage m_storage;
	AfsTestRandom      m_testRandom;
};



void AfsTestsCore()
{
	{
		AfsTestMem test { "NoFreeBlocks", Afs::MinBlockSize, 3, CaseMatch::Insensitive };
		Time now = Time::NonStrictNow();
		EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());
		test.m_afs.DispMaxNameBytes();

		Vec<Afs::DirEntry> dirEntries;
		EnsureThrow(AfsResult::OK == test.m_afs.CrackPath("/", dirEntries));
		EnsureThrow(!dirEntries.Any());
		EnsureThrow(AfsResult::NameNotInDir == test.m_afs.CrackPath("/a", dirEntries));

		ObjId dirId, fileId;
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.DirCreate(ObjId::Root, "a", Seq(), ++now, dirId));
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.FileCreate(ObjId::Root, "a", Seq(), ++now, fileId));
	}

	{
		AfsTestMem test { "OneFreeBlock", Afs::MinBlockSize, 4, CaseMatch::Insensitive };
		Time now = Time::NonStrictNow();
		EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));
		EnsureThrow(1 == test.m_afs.FreeSpaceBlocks());
		test.m_afs.DispMaxNameBytes();

		ObjId dirId, dummyId;
		EnsureThrow(AfsResult::OK == test.m_afs.DirCreate(ObjId::Root, "a", Seq(), ++now, dirId));
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::NameExists == test.m_afs.DirCreate(ObjId::Root, "A", Seq(), ++now, dummyId));
		EnsureThrow(AfsResult::NameExists == test.m_afs.FileCreate(ObjId::Root, "a", Seq(), ++now, dummyId));
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.FileCreate(ObjId::Root, "b", Seq(), ++now, dummyId));

		Vec<Afs::DirEntry> dirEntries;
		EnsureThrow(AfsResult::OK == test.m_afs.CrackPath("/A", dirEntries));
		EnsureThrow(1 == dirEntries.Len());
		{	Afs::DirEntry& e = dirEntries.First();
			EnsureThrow(e.m_id == dirId);
			EnsureThrow(Seq(e.m_name).EqualExact("a"));
			EnsureThrow(Afs::ObjType::Dir == e.m_type); }

		ObjId fileId;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjDelete(ObjId::Root, "A", ++now));
		EnsureThrow(1 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::NameNotInDir == test.m_afs.CrackPath("/a", dirEntries));
		EnsureThrow(AfsResult::OK == test.m_afs.FileCreate(ObjId::Root, "a", Seq(), ++now, fileId));
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.FileCreate(ObjId::Root, "b", Seq(), ++now, dummyId));
		EnsureThrow(AfsResult::OK == test.m_afs.CrackPath("/A", dirEntries));
		{	Afs::DirEntry& e = dirEntries.First();
			EnsureThrow(e.m_id == fileId);
			EnsureThrow(fileId != dirId);
			EnsureThrow(Seq(e.m_name).EqualExact("a"));
			EnsureThrow(Afs::ObjType::File == e.m_type); }
	}

	{
		AfsTestMem test { "TwoFreeBlocks", Afs::MinBlockSize, 5, CaseMatch::Insensitive };
		Time now = Time::NonStrictNow();
		EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));
		EnsureThrow(2 == test.m_afs.FreeSpaceBlocks());
		test.m_afs.DispMaxNameBytes();

		ObjId dirId, dummyId;
		EnsureThrow(AfsResult::OK == test.m_afs.DirCreate(ObjId::Root, "a", Seq(), ++now, dirId));
		EnsureThrow(1 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::NameExists == test.m_afs.DirCreate(ObjId::Root, "A", Seq(), ++now, dummyId));

		Afs::StatInfo dirInfo;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(dirId, dirInfo));
		EnsureThrow(Afs::ObjType::Dir == dirInfo.m_type);
		EnsureThrow(dirId == dirInfo.m_id);
		EnsureThrow(0 == dirInfo.m_dir_nrEntries);
		EnsureThrow(dirInfo.m_createTime < now);
		EnsureThrow(dirInfo.m_modifyTime == dirInfo.m_createTime);
		EnsureThrow(!dirInfo.m_metaData.Any());

		ObjId fileId;
		EnsureThrow(AfsResult::OK == test.m_afs.FileCreate(ObjId::Root, "b", Seq(), ++now, fileId));
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.FileCreate(ObjId::Root, "c", Seq(), ++now, dummyId));

		Afs::StatInfo fileInfo1;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(fileId, fileInfo1));
		EnsureThrow(Afs::ObjType::File == fileInfo1.m_type);
		EnsureThrow(fileId == fileInfo1.m_id);
		EnsureThrow(0 == fileInfo1.m_file_sizeBytes);
		EnsureThrow(dirInfo.m_createTime < fileInfo1.m_createTime);
		EnsureThrow(fileInfo1.m_modifyTime == fileInfo1.m_createTime);
		EnsureThrow(!fileInfo1.m_metaData.Any());

		uint32 maxMiniNodeBytes;
		EnsureThrow(AfsResult::OK == test.m_afs.FileMaxMiniNodeBytes(fileId, maxMiniNodeBytes));
		Console::Out(Str("maxMiniNodeBytes: ").UInt(maxMiniNodeBytes).Add("\r\n"));

		Str allData;
		allData.Ch('1');
		EnsureThrow(AfsResult::OK == test.m_afs.FileWrite(fileId, 0, allData, ++now));

		Afs::StatInfo fileInfo2;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(fileId, fileInfo2));
		EnsureThrow(Afs::ObjType::File == fileInfo2.m_type);
		EnsureThrow(fileId == fileInfo2.m_id);
		EnsureThrow(1 == fileInfo2.m_file_sizeBytes);
		EnsureThrow(fileInfo1.m_createTime == fileInfo2.m_createTime);
		EnsureThrow(fileInfo1.m_modifyTime < fileInfo2.m_modifyTime);
		EnsureThrow(!fileInfo2.m_metaData.Any());

		bool readOk {};
		int timesCalled {};
		auto onData = [&] (Seq data, bool reachedEnd)
			{	EnsureThrow(1 == ++timesCalled);
				EnsureThrow(reachedEnd);
				readOk = data.EqualExact(allData); };
		auto checkReadOk = [&] ()
			{	EnsureThrow(readOk);
				readOk = false;
				timesCalled = 0; };
		EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, 0, 1000, onData));
		checkReadOk();

		Str data2;
		data2.Chars(maxMiniNodeBytes - 1, '2');
		EnsureThrow(AfsResult::OK == test.m_afs.FileWrite(fileId, 1, data2, ++now));
		allData.Add(data2);

		Afs::StatInfo fileInfo3;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(fileId, fileInfo3));
		EnsureThrow(maxMiniNodeBytes == fileInfo3.m_file_sizeBytes);
		EnsureThrow(fileInfo2.m_modifyTime < fileInfo3.m_modifyTime);

		EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, 0, maxMiniNodeBytes, onData));
		checkReadOk();

		Seq data3 = "3";
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.FileWrite(fileId, fileInfo3.m_file_sizeBytes, data3, ++now));
		EnsureThrow(AfsResult::OK == test.m_afs.ObjDelete(ObjId::Root, "a", ++now));
		EnsureThrow(1 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::OK == test.m_afs.FileWrite(fileId, fileInfo3.m_file_sizeBytes, data3, ++now));
		allData.Add(data3);
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());

		Afs::StatInfo fileInfo4;
		EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(fileId, fileInfo4));
		EnsureThrow(maxMiniNodeBytes + 1 == fileInfo4.m_file_sizeBytes);
		EnsureThrow(fileInfo3.m_modifyTime < fileInfo4.m_modifyTime);

		EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, 0, maxMiniNodeBytes + 1, onData));
		checkReadOk();

		uint64 actualNewSize {};
		EnsureThrow(AfsResult::OK == test.m_afs.FileSetSize(fileId, maxMiniNodeBytes, actualNewSize, ++now));
		EnsureThrow(1 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(maxMiniNodeBytes == actualNewSize);
		allData.ResizeExact(maxMiniNodeBytes);

		EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, 0, maxMiniNodeBytes, onData));
		checkReadOk();

		EnsureThrow(AfsResult::OK == test.m_afs.DirCreate(ObjId::Root, "c", Seq(), ++now, dirId));
		EnsureThrow(0 == test.m_afs.FreeSpaceBlocks());
		EnsureThrow(AfsResult::OutOfSpace == test.m_afs.DirCreate(ObjId::Root, "d", Seq(), ++now, dummyId));
	}

	{
		ObjId dirId, fileId;
		Seq const testData = "FileTest";

		{
			AfsTestFile test { "File1", 4096, UINT64_MAX, DeleteExisting::Yes, AfsFileStorage::Consistency::Journal, CaseMatch::Insensitive };
			Time now = Time::NonStrictNow();
			EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));
			test.m_afs.DispMaxNameBytes();

			EnsureThrow(AfsResult::OK == test.m_afs.DirCreate(ObjId::Root, "a", Seq(), ++now, dirId));
			EnsureThrow(AfsResult::OK == test.m_afs.FileCreate(ObjId::Root, "b", Seq(), ++now, fileId));
			EnsureThrow(AfsResult::OK == test.m_afs.FileWrite(fileId, 0, testData, ++now));
		}

		{
			// Use same name as previous test to reopen data file
			AfsTestFile test { "File1", 4096, UINT64_MAX, DeleteExisting::No, AfsFileStorage::Consistency::Journal, CaseMatch::Insensitive };
			Time now = Time::NonStrictNow();
			EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));

			Vec<Afs::DirEntry> entries;
			bool reachedEnd;
			EnsureThrow(AfsResult::OK == test.m_afs.DirRead(ObjId::Root, Seq(), entries, reachedEnd));
			EnsureThrowWithNr(2 == entries.Len(), entries.Len());
			EnsureThrow(reachedEnd);
			EnsureThrow(entries[0].m_id   == dirId);
			EnsureThrow(entries[0].m_name == "a");
			EnsureThrow(entries[0].m_type == Afs::ObjType::Dir);
			EnsureThrow(entries[1].m_id   == fileId);
			EnsureThrow(entries[1].m_name == "b");
			EnsureThrow(entries[1].m_type == Afs::ObjType::File);

			bool readOk {};
			int timesCalled {};
			auto onData = [&] (Seq data, bool reachedEnd)
				{	EnsureThrow(1 == ++timesCalled);
					EnsureThrow(reachedEnd);
					readOk = data.EqualExact(testData); };
			auto checkReadOk = [&] ()
				{	EnsureThrow(readOk);
					readOk = false;
					timesCalled = 0; };
			EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, 0, 1000, onData));
			checkReadOk();
		}
	}

	for (uint32 seed=1; seed!=1000; ++seed)
	{
		AfsTestMemRandom(seed,  250, 4000, 10000, false).Run();
		AfsTestMemRandom(seed,  500, 2000, 10000, false).Run();
		AfsTestMemRandom(seed, 1000, 1000, 10000, false).Run();

		AfsFileStorage::Consistency consistency = AfsFileStorage::Consistency::Journal;
		if (1 == (seed % 3))
			consistency = AfsFileStorage::Consistency::VerifyJournal;

		AfsTestFileRandom(seed,  4096, 1024*1024, 1000, consistency, false).Run();
		AfsTestFileRandom(seed, 16384, 1024*1024, 1000, consistency, false).Run();
	}
}


void AfsTestPerf(Seq testName, AfsFileStorage::Consistency consistency)
{
	AfsTestFile test { testName, 4096, UINT64_MAX, DeleteExisting::Yes, consistency, CaseMatch::Exact };
	Time now = Time::NonStrictNow();
	EnsureThrow(AfsResult::OK == test.m_afs.Init(Seq(), now));

	ObjId fileId;
	EnsureThrow(AfsResult::OK == test.m_afs.FileCreate(ObjId::Root, "b", Seq(), ++now, fileId));

	Str testData;
	testData.Chars(64*1024, 't');
	uint64 nrBytes, ms;

	auto displayBytesPerSec = [&nrBytes, &ms] (Seq verb)
		{
			uint64 bytesPerSec = ms ? ((1000 * nrBytes) / ms) : UINT64_MAX;
			Str msg;
			msg.UIntUnits(nrBytes, Units::Bytes).Ch(' ').Add(verb).Add(" in ").UIntUnits(ms*10000, Units::Duration)
				.Add(", ").UIntUnits(bytesPerSec, Units::Bytes).Add(" per second\r\n");
			Console::Out(msg);
		};

	nrBytes = 0;
	ms = test.m_afs.PerfTest(3000, [&] () -> bool
		{
			EnsureThrow(AfsResult::OK == test.m_afs.FileWrite(fileId, nrBytes, testData, ++now));
			nrBytes += testData.Len();
			return true;
		} );

	displayBytesPerSec("written");

	Afs::StatInfo fileInfo;
	EnsureThrow(AfsResult::OK == test.m_afs.ObjStat(fileId, fileInfo));

	nrBytes = 0;
	ms = test.m_afs.PerfTest(4000, [&] () -> bool
		{
			bool keepGoing = true;
			EnsureThrow(AfsResult::OK == test.m_afs.FileRead(fileId, nrBytes, 64*1024,
				[&] (Seq data, bool reachedEnd)
				{
					nrBytes += data.n;
					data.DropToFirstByteNotOf("t");
					EnsureThrow(!data.n);
					keepGoing = !reachedEnd;
				} ));

			return keepGoing;
		} );

	displayBytesPerSec("read");
}


void AfsTestsPerf()
{
	AfsTestPerf("FilePerfJournal", AfsFileStorage::Consistency::Journal );
	AfsTestPerf("FilePerfFlush",   AfsFileStorage::Consistency::Flush   );
	AfsTestPerf("FilePerfNoFlush", AfsFileStorage::Consistency::NoFlush );
}


void AfsTests(Args& args)
{
	Seq cmd = args.NextOrErr("Missing Afs test command. Available: core | perf");

	if (cmd.EqualInsensitive("core"))
	{
		if (args.Any())
			throw Args::Err("Unrecognized parameter");

		AfsTestsCore();
	}
	else if (cmd.EqualInsensitive("perf"))
	{
		if (args.Any())
			throw Args::Err("Unrecognized parameter");

		AfsTestsPerf();
	}
	else
		throw Args::Err("Unrecognized Afs test command");
}
