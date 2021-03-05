#pragma once

// Afs - Abstract File System: a virtual filesystem which is able to use arbitrary
// (local or remote, encrypted or plaintext, memory or disk-based) block storage,
// as long as the storage supports random access by block index. To avoid corruption
// in case of crash, the storage should support journaled or transactioned writes.
// Afs expects to access the storage exclusively; concurrent access is not supported.
// Afs itself can be called by multiple threads IF they arrange appropriate locking.

#include "AtBlockAllocator.h"
#include "AtDescEnum.h"
#include "AtObjId.h"
#include "AtRpVec.h"
#include "AtTime.h"


namespace At
{

	// AfsResult

	DESCENUM_DECL_BEGIN(AfsResult)
	DESCENUM_DECL_VALUE(OK,                           0)

	// AfsStorage errors
	DESCENUM_DECL_VALUE(OutOfSpace,              100100)
	DESCENUM_DECL_VALUE(BlockIndexInvalid,       100200)
	DESCENUM_DECL_VALUE(StorageInErrorState,     100300)
		
	// Afs errors
	DESCENUM_DECL_VALUE(UnexpectedBlockKind,     200100)
	DESCENUM_DECL_VALUE(UnsupportedFsVersion,    200200)
	DESCENUM_DECL_VALUE(InvalidObjId,            200300)
	DESCENUM_DECL_VALUE(DirNotFound,             200400)
	DESCENUM_DECL_VALUE(ObjNotFound,             200500)
	DESCENUM_DECL_VALUE(ObjNotDir,               200600)
	DESCENUM_DECL_VALUE(ObjNotFile,              200700)
	DESCENUM_DECL_VALUE(NameTooLong,             200800)
	DESCENUM_DECL_VALUE(MetaDataTooLong,         200900)
	DESCENUM_DECL_VALUE(MetaDataCannotChangeLen, 201000)
	DESCENUM_DECL_VALUE(NameInvalid,             201100)
	DESCENUM_DECL_VALUE(NameNotInDir,            201200)
	DESCENUM_DECL_VALUE(NameExists,              201300)
	DESCENUM_DECL_VALUE(InvalidPathSyntax,       201400)
	DESCENUM_DECL_VALUE(MoveDestInvalid,         201500)
	DESCENUM_DECL_VALUE(DirNotEmpty,             201600)
	DESCENUM_DECL_VALUE(FileNotEmpty,            201700)
	DESCENUM_DECL_VALUE(InvalidOffset,           201800)
	DESCENUM_DECL_CLOSE();



	// AfsBlock

	class AfsChangeTracker;
	class AfsStorage;

	struct AfsBlock : RefCountable
	{
		// tracker can be nullptr if and only if the content of the block is not changed
		AfsBlock(AfsChangeTracker* tracker) : m_changeTracker(tracker) {}

		void SetChangeTracker(AfsChangeTracker& tracker) { EnsureThrow(!ChangePending()); EnsureThrow(!m_changeTracker); m_changeTracker = &tracker; }
		void ClearChangeTracker() { EnsureThrow(!ChangePending()); m_changeTracker = nullptr; }

		void Init(AfsStorage& storage, uint64 blockIndex, Rp<RcBlock> const& dataBlock) noexcept;
		bool Inited() const { return nullptr != m_storage; }

		uint32 BlockSize() const { return m_blockSize; }
		uint64 BlockIndex() const { return m_blockIndex; }

		byte const* ReadPtr() const { return m_dataBlock->Ptr(); }

		Rp<RcBlock> GetDataBlock() { return m_dataBlock; }

		// At first write, moves m_dataBlock into m_dataBlockOrig and creates a copy in m_dataBlock
		byte* WritePtr() { if (!ChangePending()) SetChangePending(); return m_dataBlock->Ptr(); }
		bool ChangePending() const { return m_dataBlockOrig.Any(); }

		// Called by JournaledWrite::Complete()
		void OnWriteComplete();

		// Called by JournaledWrite destructor if JournaledWrite::Complete() not called
		void OnWriteAborted();

	private:
		AfsBlock(AfsBlock const&) = delete;
		AfsBlock(AfsBlock&& x) noexcept = delete;

		AfsBlock& operator= (AfsBlock const&) = delete;
		AfsBlock& operator= (AfsBlock&& x) noexcept = delete;

	protected:
		AfsChangeTracker* m_changeTracker {};
		AfsStorage*       m_storage       {};
		uint64            m_blockIndex    {};
		Rp<RcBlock>       m_dataBlock     {};
		Rp<RcBlock>       m_dataBlockOrig {};
		uint32            m_blockSize     {};

		void SetChangePending();
	};


	class AfsChangeTracker
	{
	public:
		void AddChangedBlock(Rp<AfsBlock> const& block) { m_changedBlocks.Add(block); }
	
	protected:
		RpVec<AfsBlock> m_changedBlocks;
	};


	class AfsStorage : NoCopy
	{
	public:
		// BlockAllocator block size must be equal to or greater than BlockSize()
		virtual BlockAllocator& Allocator() = 0;

		// Block size must be equal to or less than BlockAllocator block size.
		// Block size MUST NOT change at any time after first Afs initialization against a particular instance of storage
		virtual uint32 BlockSize() = 0;

		// The return value MAY change at any time, including to a lower value than returned by NrBlocks().
		// Return UINT64_MAX to indicate that the limit is unknown.
		virtual uint64 MaxNrBlocks() = 0;

		// Must return the number of blocks available in storage, plus any additional blocks that were created but not yet written.
		// When Afs has never been initialized, this must return 0.
		virtual uint64 NrBlocks() = 0;

		// Must create a new, zero-initialized block just past the current last available or created block.
		// This must cause the value returned by NrBlocks() to be incremented by one.
		// This must be called from within a journaled write. If the journaled write is aborted, the NrBlocks() value must be restored.
		// If the journaled write is completed, the block MUST be included in the call to CompleteJournaledWrite.
		// If block cannot be added because out of space, must return OutOfSpace.
		virtual AfsResult::E AddNewBlock(AfsBlock& block) = 0;

		// Must obtain an existing block that has already been written. If blockIndex is past the current last block, must return BlockIndexInvalid.
		// A block obtained with AddNewBlock must not be obtained through ObtainBlock or ObtainBlockForOverwrite until the journaled write has aborted or completed.
		virtual AfsResult::E ObtainBlock(AfsBlock& block, uint64 blockIndex) = 0;

		// Obtains an existing block, but without loading its data. If blockIndex is past the current last block, must return BlockIndexInvalid.
		// This must be called from within a journaled write. If the journaled write is completed, the block MUST be included in the call to CompleteJournaledWrite.
		// A block obtained with AddNewBlock must not be obtained through ObtainBlock or ObtainBlockForOverwrite until the journaled write has aborted or completed.
		virtual AfsResult::E ObtainBlockForOverwrite(AfsBlock& block, uint64 blockIndex) = 0;

		// Journaled writes are used to ensure filesystem consistency when blocks are written. All modifications must be done within a journaled write.
		// This method may attempt to perform recovery from a previous storage failure. If the recovery attempt fails, this method may throw.
		virtual void BeginJournaledWrite() = 0;

		// To call AbortJournaledWrite(), there must have been a call to BeginJournaledWrite() without a corresponding call to CompleteJournaledWrite(),
		// or the call to CompleteJournaledWrite() must have thrown an exception.
		virtual void AbortJournaledWrite() noexcept = 0;

		// If CompleteJournaledWrite() throws an exception, the journaled write must be aborted using AbortJournaledWrite().
		// Depending on the nature of the error, the AfsStorage instance may or may not continue to function.
		// If the error is transient, the AfsStorage instance may continue to throw exceptions for an amount of time, and later recover.
		virtual void CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite) = 0;
	};



	class Afs
	{
	public:
		typedef int (*NameComparer)(Seq, Seq);

		// Initialization functions. Must not be called after the filesystem has been initialized
		void SetStorage(AfsStorage& storage);
		void SetNameComparer(NameComparer cmp);


		// OurFsVersion == "AFS0"
		// Minimum block size is the first block size such that the calculation in Init() produces a non-zero MaxNameBytes()
		enum { OurFsVersion = 0x30534641, MinBlockSize = 144 };

		AfsResult::E Init(Seq rootDirMetaData, Time now);

		uint32 MaxNameBytes() const { EnsureThrow(State::Inited == m_state); return m_maxNameBytes; }
		uint32 MaxMetaBytes() const { EnsureThrow(State::Inited == m_state); return m_maxMetaBytes; }
		AfsResult::E CheckName(Seq name) const { EnsureThrow(State::Inited == m_state); return CheckName_Inner(name); }

		uint64 FreeSpaceBlocks();
		uint64 FreeSpaceBytes() { return SatMul<uint64>(FreeSpaceBlocks(), m_blockSize); }

		void VerifyFreeList();


		struct ObjType { enum E : byte { Any = 0, Dir = 1, File = 2 }; };

		struct StatField { enum E { CreateTime = 0x01, ModifyTime = 0x02, MetaData = 0x04 }; };

		struct StatInfo
		{
			ObjType::E m_type           {};
			ObjId      m_id             {};
			ObjId      m_parentId       {};
			uint64     m_dir_nrEntries  { UINT64_MAX };	// If directory, otherwise UINT64_MAX
			uint64     m_file_sizeBytes { UINT64_MAX };	// If file, otherwise UINT64_MAX
			Time       m_createTime     {};
			Time       m_modifyTime     {};
			Str        m_metaData       {};
		};


		struct DirEntry
		{
			ObjId      m_id   {};
			ObjType::E m_type {};
			Str        m_name;
		};

		// If "parentDirId" could not have been valid, returns InvalidObjId.
		// If "parentDirId" could have referred to a directory before, but does not now, e.g. because it was removed, returns DirNotFound.
		// If "parentDirId" refers to a file, returns ObjNotDir.
		// If "name" is not found in directory, returns NameNotInDir.
		AfsResult::E FindNameInDir(ObjId parentDirId, Seq name, DirEntry& entry);

		// If "absPath" has invalid syntax, returns InvalidPathSyntax.
		// "absPath" must begin with "/", must NOT contain multiple consecutive "/", and MAY end in a final "/" which is ignored.
		// Root directory is NOT included in results. The output of CrackPath("/") is therefore an empty container.
		// If lookup fails at any point, returns a partially constructed result.
		// If the return value is not OK, and partial result is returned, then the error refers to the next entry that would have been looked up.
		AfsResult::E CrackPath(Seq absPath, Vec<DirEntry>& entries);

		// If "id" could not have been valid, returns InvalidObjId.
		// If "id" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		AfsResult::E ObjStat(ObjId id, StatInfo& info);

		// If "id" could not have been valid, returns InvalidObjId.
		// If "id" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// "statFields" is a bitwise combination of StatField flags. Only the indicated fields are modified.
		// If metadata is changed, it must maintain the same length as before. If there is an attempt to change metadata length, returns MetaDataCannotChangeLen.
		// If storage is exhausted, returns OutOfSpace.
		AfsResult::E ObjSetStat(ObjId id, StatInfo const& info, uint32 statFields);

		// If "parentDirId" could not have been valid, returns InvalidObjId.
		// If "parentDirId" could have referred to a directory before, but does not now, e.g. because it was removed, returns DirNotFound.
		// If "parentDirId" refers to a file, returns ObjNotDir.
		// If "name" is not found in directory, returns NameNotInDir.
		// If "name" refers to a directory and the directory is not empty, returns DirNotEmpty.
		// If storage is exhausted, returns OutOfSpace. Deletion of one name can cause a longer name to take its place in the directory tree, requiring more storage.
		AfsResult::E ObjDelete(ObjId parentDirId, Seq name, Time now);

		// If "nameNew" is too long, returns NameTooLong. If "nameNew" contains an invalid byte, returns NameInvalid.
		// If "parentDirIdOld" or "parentDirIdNew" could not have been valid, returns InvalidObjId.
		// If "parentDirIdOld" or "parentDirIdNew" could have referred to a directory before, but does not now, e.g. because it was removed, returns DirNotFound.
		// If "parentDirIdOld" or "parentDirIdNew" refers to a file, returns ObjNotDir.
		// If "nameOld" is not found in the old directory, returns NameNotInDir.
		// If "nameNew" is too long or invalid, returns NameTooLong or NameInvalid.
		// If "nameNew" already exists in the new directory, returns NameExists.
		// If storage is exhausted, returns OutOfSpace.
		AfsResult::E ObjMove(ObjId parentDirIdOld, Seq nameOld, ObjId parentDirIdNew, Seq nameNew, Time now);

		// If "parentDirId" could not have been valid, returns InvalidObjId.
		// If "parentDirId" could have referred to a directory before, but does not now, e.g. because it was removed, returns DirNotFound.
		// If "parentDirId" refers to a file, returns ObjNotDir.
		// If "name" is too long or invalid, returns NameTooLong or NameInvalid.
		// If "name" already exists in the directory, returns NameExists.
		// If "metaData" is too long, returns MetaDataTooLong.
		// If storage is exhausted, returns OutOfSpace.
		AfsResult::E DirCreate(ObjId parentDirId, Seq name, Seq metaData, Time now, ObjId& dirId);

		// If "dirId" could not have been valid, returns InvalidObjId.
		// If "dirId" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// If "dirId" refers to a file, returns ObjNotDir.
		// The "entries" Vec is NOT cleared before adding entries.
		AfsResult::E DirRead(ObjId dirId, Seq lastNameRead, Vec<DirEntry>& entries, bool& reachedEnd);

		// If "parentDirId" could not have been valid, returns InvalidObjId.
		// If "parentDirId" could have referred to a directory before, but does not now, e.g. because it was removed, returns DirNotFound.
		// If "parentDirId" refers to a file, returns ObjNotDir.
		// If "name" is too long or invalid, returns NameTooLong or NameInvalid.
		// If "name" already exists in the directory, returns NameExists.
		// If "metaData" is too long, returns MetaDataTooLong.
		// If storage is exhausted, returns OutOfSpace.
		AfsResult::E FileCreate(ObjId parentDirId, Seq name, Seq metaData, Time now, ObjId& fileId);

		// If "fileId" could not have been valid, returns InvalidObjId.
		// If "fileId" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// If "fileId" refers to a directory, returns ObjNotFile.
		AfsResult::E FileMaxMiniNodeBytes(ObjId fileId, uint32& maxMiniNodeBytes);

		// If "fileId" could not have been valid, returns InvalidObjId.
		// If "fileId" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// If "fileId" refers to a directory, returns ObjNotFile.
		// If "offset" exceeds file size, returns InvalidOffset.
		// If "offset" plus "n" exceeds file size, "n" is internally reduced.
		// If "offset" is exactly equal to file size, "onData" is called with zero bytes and "reachedEnd" == true.
		// If "n" is at least 2, "onData" may be called multiple times.
		// If end of file is reached, the last call to "onData" will have "reachedEnd" == true.
		AfsResult::E FileRead(ObjId fileId, uint64 offset, sizet n, std::function<void(Seq, bool reachedEnd)> onData);

		// If "fileId" could not have been valid, returns InvalidObjId.
		// If "fileId" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// If "fileId" refers to a directory, returns ObjNotFile.
		// If "offset" plus "data.n" exceeds file size, the file is appropriately enlarged.
		// If "data.n" is zero, but offset is past end of file, the file is still appropriately enlarged.
		// If storage is exhausted, returns OutOfSpace.
		AfsResult::E FileWrite(ObjId fileId, uint64 offset, Seq data, Time now);

		// If "fileId" could not have been valid, returns InvalidObjId.
		// If "fileId" could have referred to a file or directory before, but does not now, e.g. because it was removed, returns ObjNotFound.
		// If "fileId" refers to a directory, returns ObjNotFile.
		// If new size is larger and storage is exhausted, returns OutOfSpace. In this case, actualNewSize is set to the size actually achieved.
		// The last file modification time is updated even if size does not change.
		AfsResult::E FileSetSize(ObjId fileId, uint64 newSizeBytes, uint64& actualNewSize, Time now);


	private:
		AfsStorage*  m_storage {};
		NameComparer m_cmp     {};

		enum class State { Uninited, Inited, Error };
		State            m_state   {};


		struct StateGuard
		{
			StateGuard(Afs& afs) : m_afs(afs) {}
			~StateGuard() { if (!m_dismissed) m_afs.m_state = State::Error; }

			AfsResult::E Dismiss(AfsResult::E r) { m_dismissed = true; return r; }

		private:
			Afs& m_afs;
			bool m_dismissed {};
		};


		struct BlockKind { enum E : byte { None = 0, Master = 1, FreeList = 2, FreeBlock = 3, Node = 4 }; };

		struct VarBlock;

		struct JournaledWrite : AfsChangeTracker
		{
			JournaledWrite(Afs& afs);
			~JournaledWrite();

			void IncrementFinalizationsPending() { ++m_nrFinalizationsPending; }
			void DecrementFinalizationsPending() { EnsureThrow(0 != m_nrFinalizationsPending); --m_nrFinalizationsPending; }

			void AddBlockToFree(Rp<VarBlock> const& block);
			void Complete();

			Rp<VarBlock> ReclaimBlockOrAddNew(BlockKind::E blockKind);

		private:
			Afs&            m_afs;
			sizet           m_nrFinalizationsPending {};
			RpVec<VarBlock> m_blocksToFree;
			Rp<VarBlock>    m_newFreeListTailBlock;
			bool            m_completed {};

			Rp<VarBlock> TryReclaimBlock();
			VarBlock& GetNewFreeListTailBlock();
		};


		struct BlockWithKind : AfsBlock
		{
			BlockWithKind(AfsChangeTracker* tracker) : AfsBlock(tracker) {}

			AfsResult::E ObtainBlock_CheckKind(AfsStorage* storage, uint64 blockIndex, BlockKind::E kind);

			// One of BlockKind
			byte           GetBlockKind() const { return ReadPtr() [0]; }
			byte volatile& SetBlockKind()       { return WritePtr()[0]; }
		};


		struct MasterBlock : BlockWithKind
		{
			MasterBlock(AfsChangeTracker* tracker) : BlockWithKind(tracker) {}

			uint64           GetFsVersion              () const { return ((uint64*) ReadPtr() )[1]; }
			uint64 volatile& SetFsVersion              ()       { return ((uint64*) WritePtr())[1]; }

			uint64           GetFreeListTailBlockIndex () const { return ((uint64*) ReadPtr() )[2]; }
			uint64 volatile& SetFreeListTailBlockIndex ()       { return ((uint64*) WritePtr())[2]; }

			uint64           GetNrFullFreeListNodes    () const { return ((uint64*) ReadPtr() )[3]; }
			uint64 volatile& SetNrFullFreeListNodes    ()       { return ((uint64*) WritePtr())[3]; }

			uint64           GetRootDirTopNodeIndex    () const { return ((uint64*) ReadPtr() )[4]; }
			uint64 volatile& SetRootDirTopNodeIndex    ()       { return ((uint64*) WritePtr())[4]; }

			uint64           GetNextUniqueId           () const { return ((uint64*) ReadPtr() )[5]; }
			uint64 volatile& SetNextUniqueId           ()       { return ((uint64*) WritePtr())[5]; }
		};


		struct View
		{
			View(VarBlock& block, uint32 offset, uint32 minLenBytes) : m_block(block), m_offset(offset), m_len(block.BlockSize() - offset)
				{ EnsureThrowWithNr2(offset + minLenBytes <= block.BlockSize(), offset, minLenBytes); }

			uint32 ViewLen() const { return m_len; }

			byte const* ReadPtr  () const { return m_block.ReadPtr()  + m_offset; }
			byte*       WritePtr ()       { return m_block.WritePtr() + m_offset; }

		protected:
			VarBlock&    m_block;
			uint32 const m_offset;
			uint32 const m_len;
		};


		struct FreeListView : View
		{
			FreeListView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 8U), MinLenBytes) { EnsureThrow(0 == m_offset % 8); }

			enum { FixedFieldsBytes = 16, MinLenBytes = FixedFieldsBytes + 8 };

			uint64           GetPrevFreeListBlockIndex () const { return ((uint64*) ReadPtr() )[0]; }		// UINT64_MAX if none
			uint64 volatile& SetPrevFreeListBlockIndex ()       { return ((uint64*) WritePtr())[0]; }		// UINT64_MAX if none

			uint32           GetNrIndices              () const { return ((uint32*) ReadPtr() )[2]; }
			uint32 volatile& SetNrIndices              ()       { return ((uint32*) WritePtr())[2]; }

			uint64           GetFreeBlockIndex         (uint32 i) const { return ((uint64*) ReadPtr() )[2+i]; }
			uint64 volatile& SetFreeBlockIndex         (uint32 i)       { return ((uint64*) WritePtr())[2+i]; }

			static uint32 MaxNrIndices(uint32 blockSize) { return SatSub<uint32>(blockSize, 24) / 8; }
		};


		struct DirLeafEntry
		{
			ObjId      m_id   {};
			ObjType::E m_type {};
			Seq        m_name;

			enum { EncodedSizeOverhead = 16 + 1 + 2 };
			uint32 EncodedSize() const { return EncodedSizeOverhead + (uint32) m_name.n; }
		};


		struct DirLeafView : View
		{
			DirLeafView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 4U), FixedFieldsBytes) { EnsureThrow(0 == m_offset % 4); }

			enum { FixedFieldsBytes = 4 };

			uint32           GetNrEntries() const { return ((uint32*) ReadPtr() )[0]; }
			uint32 volatile& SetNrEntries()       { return ((uint32*) WritePtr())[0]; }

			bool Any() const { return GetNrEntries() != 0; }

			void DecodeEntries(Vec<DirLeafEntry>& entries);
			void EncodeEntries(Vec<DirLeafEntry> const& entries);

			uint32 EncodedSizeOverhead() const { return m_offset + FixedFieldsBytes; }
			uint32 EncodedSizeEntries(Vec<DirLeafEntry> const& entries) const;		// Encoded size for the entries only
		};


		struct DirBranchEntry
		{
			uint64 m_blockIndex {};		// Block index of next lower-level node (leaf or branch)
			Seq    m_name;				// First name in the lower-level node

			enum { EncodedSizeOverhead = 8 + 2 };
			uint32 EncodedSize() const { return EncodedSizeOverhead + (uint32) m_name.n; }
		};


		struct DirBranchView : View
		{
			DirBranchView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 4U), FixedFieldsBytes) { EnsureThrow(0 == m_offset % 4); }

			enum { FixedFieldsBytes = 4 };

			uint32           GetNrEntries() const { return ((uint32*) ReadPtr ())[0]; }
			uint32 volatile& SetNrEntries()       { return ((uint32*) WritePtr())[0]; }

			bool Any() const { return GetNrEntries() != 0; }

			void DecodeEntries(Vec<DirBranchEntry>& entries);
			void EncodeEntries(Vec<DirBranchEntry> const& entries);

			uint32 EncodedSizeOverhead() const { return m_offset + FixedFieldsBytes; }
			uint32 EncodedSizeEntries(Vec<DirBranchEntry> const& entries) const;	// Encoded size for the entries only
		};


		struct DirView : View
		{
			DirView(VarBlock& block, uint32 offset) : View(block, offset, FieldsBytes) {}

			enum { FieldsBytes = 2 };

			// Zero if leaf node, non-zero for branch nodes
			byte           GetDirNodeLevel() const { return ReadPtr ()[0]; }
			byte volatile& SetDirNodeLevel()       { return WritePtr()[0]; }

			bool IsLeaf   () const { return GetDirNodeLevel() == 0; }
			bool IsBranch () const { return GetDirNodeLevel() >  0; }

			DirLeafView   AsLeafView      () const { EnsureThrow(IsLeaf());   return DirLeafView   (m_block, m_offset + FieldsBytes); }
			DirLeafView   AsIf_LeafView   () const {                          return DirLeafView   (m_block, m_offset + FieldsBytes); }
			DirBranchView AsBranchView    () const { EnsureThrow(IsBranch()); return DirBranchView (m_block, m_offset + FieldsBytes); }
			DirBranchView AsIf_BranchView () const {                          return DirBranchView (m_block, m_offset + FieldsBytes); }

			bool Any() const { return IsLeaf() ? AsLeafView().Any() : AsBranchView().Any(); }
		};


		struct FileLeafEntry
		{
			uint64 m_blockIndex {};

			enum { EncodedSize = 8 };
		};


		struct FileLeafView : View
		{
			FileLeafView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 8U), FixedFieldsBytes) { EnsureThrow(0 == m_offset % 8); }

			enum { FixedFieldsBytes = 12 };

			uint64           GetFileOffset () const { return ((uint64*) ReadPtr ())[0]; }
			uint64 volatile& SetFileOffset ()       { return ((uint64*) WritePtr())[0]; }

			uint32           GetNrEntries  () const { return ((uint32*) ReadPtr ())[2]; }
			uint32 volatile& SetNrEntries  ()       { return ((uint32*) WritePtr())[2]; }

			void DecodeEntries(Vec<FileLeafEntry>& entries);
			void EncodeEntries(Vec<FileLeafEntry> const& entries);

			uint32 EncodedSizeOverhead() const { return m_offset + FixedFieldsBytes; }
			uint32 EncodedSizeEntries(Vec<FileLeafEntry> const& entries) const;		// Encoded size for the entries only
		};


		struct FileBranchEntry
		{
			uint64 m_fileOffset {};
			uint64 m_blockIndex {};

			enum { EncodedSize = 16 };
		};


		struct FileBranchView : View
		{
			FileBranchView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 4U), FixedFieldsBytes) { EnsureThrow(0 == m_offset % 4); }

			enum { FixedFieldsBytes = 4 };

			uint32           GetNrEntries() const { return ((uint32*) ReadPtr ())[0]; }
			uint32 volatile& SetNrEntries()       { return ((uint32*) WritePtr())[0]; }

			void DecodeEntries(Vec<FileBranchEntry>& entries);
			void EncodeEntries(Vec<FileBranchEntry> const& entries);

			uint32 EncodedSizeOverhead() const { return m_offset + FixedFieldsBytes; }
			uint32 EncodedSizeEntries(Vec<FileBranchEntry> const& entries) const;	// Encoded size for the entries only
		};


		struct FileMiniView : View
		{
			FileMiniView(VarBlock& block, uint32 offset) : View(block, offset, 1) {}
		};


		enum { NodeLevel_BeyondMax = 255 };

		struct FileView : View
		{
			FileView(VarBlock& block, uint32 offset) : View(block, offset, FieldsBytes) {}

			enum { FieldsBytes = 1 };

			// Zero if leaf node, non-zero for branch nodes, NodeLevel_BeyondMax for small-file mini-node
			byte           GetFileNodeLevel() const { return ReadPtr ()[0]; }
			byte volatile& SetFileNodeLevel()       { return WritePtr()[0]; }

			bool IsLeaf   () const { return GetFileNodeLevel() == 0; }
			bool IsBranch () const { return GetFileNodeLevel() > 0 && GetFileNodeLevel() < NodeLevel_BeyondMax; }
			bool IsMini   () const { return GetFileNodeLevel() == NodeLevel_BeyondMax; }

			FileLeafView   AsLeafView      () const { EnsureThrow(IsLeaf());   return FileLeafView   (m_block, m_offset + FieldsBytes); }
			FileLeafView   AsIf_LeafView   () const {                          return FileLeafView   (m_block, m_offset + FieldsBytes); }
			FileBranchView AsBranchView    () const { EnsureThrow(IsBranch()); return FileBranchView (m_block, m_offset + FieldsBytes); }
			FileBranchView AsIf_BranchView () const {                          return FileBranchView (m_block, m_offset + FieldsBytes); }
			FileMiniView   AsMiniView      () const { EnsureThrow(IsMini());   return FileMiniView   (m_block, m_offset + FieldsBytes); }
			FileMiniView   AsIf_MiniView   () const {                          return FileMiniView   (m_block, m_offset + FieldsBytes); }
		};


		struct TopView : View
		{
			TopView(VarBlock& block, uint32 offset) : View(block, RoundUpToMultipleOf(offset, 8U), FixedFieldsBytes) { EnsureThrow(0 == m_offset % 8); }

			enum { FixedFieldsBytes = 50 };

			uint64           GetUniqueId        () const { return ((uint64*) ReadPtr ())[0]; }
			uint64 volatile& SetUniqueId        ()       { return ((uint64*) WritePtr())[0]; }

			uint64           GetParentUniqueId  () const { return ((uint64*) ReadPtr ())[1]; }
			uint64 volatile& SetParentUniqueId  ()       { return ((uint64*) WritePtr())[1]; }

			uint64           GetParentIndex     () const { return ((uint64*) ReadPtr ())[2]; }
			uint64 volatile& SetParentIndex     ()       { return ((uint64*) WritePtr())[2]; }

			// If directory
			uint64           Get_Dir_NrEntries  () const { return ((uint64*) ReadPtr ())[3]; }
			uint64 volatile& Set_Dir_NrEntries  ()       { return ((uint64*) WritePtr())[3]; }

			// If file
			uint64           Get_File_SizeBytes () const { return ((uint64*) ReadPtr ())[3]; }
			uint64 volatile& Set_File_SizeBytes ()       { return ((uint64*) WritePtr())[3]; }

			uint64           GetCreateTime      () const { return ((uint64*) ReadPtr ())[4]; }
			uint64 volatile& SetCreateTime      ()       { return ((uint64*) WritePtr())[4]; }
			
			uint64           GetModifyTime      () const { return ((uint64*) ReadPtr ())[5]; }
			uint64 volatile& SetModifyTime      ()       { return ((uint64*) WritePtr())[5]; }
			
			uint16           GetMetaDataLen     () const { return ((uint16*) ReadPtr ())[24]; }
			uint16 volatile& SetMetaDataLen     ()       { return ((uint16*) WritePtr())[24]; }

			ObjId GetObjId() const { return ObjId(GetUniqueId(), m_block.BlockIndex()); }

			ObjId GetParentId() const { return ObjId(GetParentUniqueId(), GetParentIndex()); }
			void SetParentId(ObjId x) { SetParentUniqueId() = x.m_uniqueId; SetParentIndex() = x.m_index; }

			Seq GetMetaData() const
			{
				sizet metaDataLen = GetMetaDataLen();
				EnsureThrowWithNr2(FixedFieldsBytes + metaDataLen <= m_len, metaDataLen, m_len);
				return Seq(ReadPtr() + FixedFieldsBytes, metaDataLen);
			}

			void SetMetaData(Seq s)
			{
				EnsureThrowWithNr(s.n <= UINT16_MAX, s.n);
				EnsureThrowWithNr2(FixedFieldsBytes + s.n <= m_len, s.n, m_len);
				SetMetaDataLen() = (uint16) s.n;
				memcpy(WritePtr() + FixedFieldsBytes, s.p, s.n);
			}
			
			DirView  AsDirView  () const { return DirView  (m_block, m_offset + FixedFieldsBytes + GetMetaDataLen()); }
			FileView AsFileView () const { return FileView (m_block, m_offset + FixedFieldsBytes + GetMetaDataLen()); }
		};


		struct NodeCat { enum E : byte { Invalid = 0, Top = 1, NonTop = 2 }; };

		struct NodeView : View
		{
			NodeView(VarBlock& block, uint32 offset) : View(block, offset, FieldsBytes) {}

			enum { FieldsBytes = 2 };

			// One of NodeCat
			byte           GetNodeCat () const { return ReadPtr() [0]; }
			byte volatile& SetNodeCat ()       { return WritePtr()[0]; }

			// One of ObjType
			byte           GetObjType () const { return ReadPtr() [1]; }
			byte volatile& SetObjType ()       { return WritePtr()[1]; }

			DirView  AsNonTopDirView  () { EnsureThrow(GetNodeCat() == NodeCat::NonTop && GetObjType() == ObjType::Dir ); return DirView  (m_block, m_offset + FieldsBytes); }
			FileView AsNonTopFileView () { EnsureThrow(GetNodeCat() == NodeCat::NonTop && GetObjType() == ObjType::File); return FileView (m_block, m_offset + FieldsBytes); }
			TopView  AsTopView        () { EnsureThrow(GetNodeCat() == NodeCat::Top);                                     return TopView  (m_block, m_offset + FieldsBytes); }

			DirView  AsAnyDirView     () { EnsureThrow(GetObjType() == ObjType::Dir ); return GetNodeCat() == NodeCat::Top ? AsTopView().AsDirView () : AsNonTopDirView  (); }
			FileView AsAnyFileView    () { EnsureThrow(GetObjType() == ObjType::File); return GetNodeCat() == NodeCat::Top ? AsTopView().AsFileView() : AsNonTopFileView (); }
		};


		struct VarBlock : BlockWithKind
		{
			VarBlock(AfsChangeTracker* tracker) : BlockWithKind(tracker) {}

			NodeView     AsNodeView     () { EnsureThrow(GetBlockKind() == BlockKind::Node);     return NodeView     (*this, 1); }
			FreeListView AsFreeListView () { EnsureThrow(GetBlockKind() == BlockKind::FreeList); return FreeListView (*this, 1); }
		};


		Rp<VarBlock>      m_rootDirTopNode;
		Rp<MasterBlock>   m_masterBlock;
		Rp<VarBlock>      m_freeListTailBlock;

		uint32            m_blockSize    {};
		uint32            m_maxNameBytes {};
		uint32            m_maxMetaBytes {};

		enum : uint32 { NodeRebalanceThresholdFraction = 4U };

		enum class FindResult { NoEntries, FirstIsGreater, FoundEqual, FoundLessThan };

		template <class EntryType>
		FindResult FindNameEqualOrLessThan(Vec<EntryType> const& entries, Seq name, sizet& foundPos);


		enum class NodeState { Initial, Changed, Free, Finalized };	// Can change from lower to higher, cannot change from higher to lower

		struct Node : RefCountable
		{
			Rp<VarBlock> m_block;
			bool         m_isTop {};
			uint32       m_level {};
			NodeState    m_state {};
		};


		struct DirNode : Node
		{
			Vec<DirLeafEntry>   m_leafEntries;
			Vec<DirBranchEntry> m_branchEntries;
			Vec<DirNode*>       m_childNodes;

			sizet NrVecEntries() const { return m_level ? m_branchEntries.Len() : m_leafEntries.Len(); }
			bool AnyVecEntries() const { return 0 != NrVecEntries(); }

			Seq FirstName() const { return m_level ? m_branchEntries.First().m_name : m_leafEntries.First().m_name; }

			void Decode();
			void Encode();

			uint32 EncodedSizeOverhead() { return AsIf_EncodedSizeOverhead(m_level); }
			uint32 AsIf_EncodedSizeOverhead(uint32 level);
			uint32 EncodedSizeEntries();
			uint32 EncodedSize() { return EncodedSizeOverhead() + EncodedSizeEntries(); }
		};


		AfsResult::E GetTopBlock(VarBlock& block, ObjId id, ObjType::E expectType);


		enum { NavPath_MaxEntries = 64 };

		struct DirNavEntry
		{
			DirNode* m_node {};
			sizet    m_pos  {};
		};

		using DirNavPath = VecFix<DirNavEntry, NavPath_MaxEntries>;

		enum class StopEarly { No, IfCantFind };


		struct CxBase
		{
			JournaledWrite* m_jw;
			bool            m_anyChanged {};

			void ChangeState(Node& node, NodeState newState);
		};


		struct DirCxR : CxBase
		{
			DirNode* m_topNode {};

			DirCxR(Afs& afs) : m_afs(afs) {}
			DirCxR(DirCxR&& x) = default;

			AfsResult::E GetDirTopBlock(ObjId id);
			FindResult NavToLeafEntryEqualOrLessThan(DirNavPath& navPath, Seq name, StopEarly stopEarly);
			DirLeafEntry& GetLeafEntryAt(DirNavPath& navPath);
			bool NavToSiblingNode(DirNavPath& navPath, EnumDir enumDir);

		protected:
			Afs&            m_afs;
			RpVec<DirNode>  m_nodes;

			void DescendToNextChildNode(DirNavPath& navPath, EnumDir enumDir);
		};


		enum class CanAddNode { No, Yes };

		struct DirCxRW : DirCxR
		{
			DirCxRW(Afs& afs, JournaledWrite& jw)
				: DirCxR(afs) { m_jw = &jw; }

			void RemoveLeafEntryAt(DirNavPath& navPath, ObjId leafEntryId, Time now);
			void AddLeafEntry(DirLeafEntry const& entry, Time now);
			void AddLeafEntryAt(DirLeafEntry const& entry, DirNavPath& navPath, Time now, CanAddNode canAddNode);
			void Finalize();

		protected:
			void RemoveNonTopNode(DirNavPath& navPath);
			void OnEntryRemoved_Maintenance(DirNavPath& navPath);
			void UpdateAncestors(DirNavPath& navPath);
			bool TryHoistIntoTopNode(DirNavPath& navPath);
			void JoinSiblingNodes(DirNavPath& navPathTo, DirNavPath& navPathFrom);
			void SplitNode(DirNavPath& navPath);														// Updates navPath to point to same entry after split
			void SplitTopNode(DirNavPath& navPath, DirNode& node, sizet const splitIndex);				// Updates navPath to point to same entry after split
			void SplitNonTopNode(DirNavPath& navPath, DirNode& node, sizet const splitIndex);			// Updates navPath to point to same entry after split
			void AddBranchEntryAt(DirBranchEntry& entry, DirNavPath& navPath, DirNode* newNode, CanAddNode canAddNode);	// If node splits, updates navPath to point to added entry
			void RebuildNavPath(DirNavPath& navPath, DirNode& node, sizet pos);
		};


		enum { FileSetSize_MaxBlocksPerRound = 100 };

		AfsResult::E CheckName_Inner(Seq name) const;
		AfsResult::E FindNameInDir_Inner(DirCxR& dirCx, ObjId parentDirId, Seq name, DirNavPath& navPath, DirEntry& entry);
		AfsResult::E ObjDelete_Inner(ObjId parentDirId, Seq name, Time now, ObjId& objId);
		AfsResult::E FileSetSize_Inner(ObjId fileId, uint64 newSizeBytes, uint64& actualNewSize, Time now);


		struct FileNode : Node
		{
			Vec<FileLeafEntry>   m_leafEntries;
			RpVec<VarBlock>      m_dataBlocks;			// Corresponding to m_leafEntries

			Vec<FileBranchEntry> m_branchEntries;
			Vec<FileNode*>       m_childNodes;			// Corresponding to m_branchEntries

			sizet NrVecEntries() const { return m_level ? m_branchEntries.Len() : m_leafEntries.Len(); }
			bool AnyVecEntries() const { return 0 != NrVecEntries(); }

			void Decode();
			void Encode();

			uint32 EncodedSizeOverhead() { return AsIf_EncodedSizeOverhead(m_level); }
			uint32 AsIf_EncodedSizeOverhead(uint32 level);
			uint32 EncodedSizeEntries();
			uint32 EncodedSize() { return EncodedSizeOverhead() + EncodedSizeEntries(); }
		};


		struct FileNavEntry
		{
			FileNode* m_node {};
			sizet     m_pos  {};
		};

		using FileNavPath = VecFix<FileNavEntry, NavPath_MaxEntries>;


		struct FileCxR : CxBase
		{
			FileNode* m_topNode {};

			FileCxR(Afs& afs) : m_afs(afs) {}
			FileCxR(FileCxR&& x) = default;

			AfsResult::E GetFileTopBlock(ObjId id);
			void GetDataBlocks(uint64 offsetFirst, uint64 offsetBeyondLast, RpVec<VarBlock>& blocks);

		protected:
			Afs&            m_afs;
			RpVec<FileNode> m_nodes;

			void NavToLeafEntryContainingOffset(FileNavPath& navPath, uint64 offset);
			void DescendToNextChildNode(FileNavPath& navPath, EnumDir enumDir);
			bool NavToSiblingNode(FileNavPath& navPath, EnumDir enumDir);
			Rp<VarBlock>& GetDataBlock(FileNode& node, sizet i);
		};


		struct FileCxRW : FileCxR
		{
			FileCxRW(Afs& afs, JournaledWrite& jw) : FileCxR(afs) { m_jw = &jw; }

			uint64 ImpliedCapacity(uint64 sizeBytes) const;
			void EnlargeToSize(uint64 newSizeBytes);		// Does NOT zero out the new space since the next action is usually a write
			void ShrinkToSize(uint64 newSizeBytes);			// Zeroes out removed space
			void Finalize();

		protected:
			void AddLeafEntryAtEnd(Rp<VarBlock> const& dataBlock, FileNavPath& navPath, CanAddNode canAddNode);
			void MakeRoomForEntryAtEnd(FileNavPath& navPath, uint64 newBlockOffset, sizet entrySize);			// Updates navPath to point to new node, if one was added. Position ignored
			sizet SplitTopNode(FileNavPath& navPath, FileNode& node);											// Updates navPath to point to new node. Position ignored
			void AddNonTopNodeAtEnd(FileNavPath& navPath, uint64 newBlockOffset);								// Updates navPath to point to new node. Position ignored
			void AddBranchEntryAtEnd(FileBranchEntry& entry, FileNavPath& navPath, FileNode* newNode, CanAddNode canAddNode);		// Updates navPath to point to added entry
			void RemoveDataBucketAtEnd(FileNavPath& navPath);
			void RemoveNodeAtEnd_NavToPrevSibling(FileNavPath& navPath);
			bool TryHoistIntoTopNode(FileNavPath& navPath);
		};

	};

}
