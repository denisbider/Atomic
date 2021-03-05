#include "AtIncludes.h"
#include "AtAfs.h"

#include "AtAuto.h"
#include "AtEncode.h"
#include "AtMap.h"


namespace At
{

	// AfsResult

	DESCENUM_DEF_BEGIN(AfsResult)
	DESCENUM_DEF_VALUE(OK)

	// AfsStorage errors
	DESCENUM_DEF_VALUE(OutOfSpace)
	DESCENUM_DEF_VALUE(BlockIndexInvalid)
	DESCENUM_DEF_VALUE(StorageInErrorState)
		
	// Afs errors
	DESCENUM_DEF_VALUE(UnexpectedBlockKind)
	DESCENUM_DEF_VALUE(UnsupportedFsVersion)
	DESCENUM_DEF_VALUE(InvalidObjId)
	DESCENUM_DEF_VALUE(DirNotFound)
	DESCENUM_DEF_VALUE(ObjNotFound)
	DESCENUM_DEF_VALUE(ObjNotDir)
	DESCENUM_DEF_VALUE(ObjNotFile)
	DESCENUM_DEF_VALUE(NameTooLong)
	DESCENUM_DEF_VALUE(MetaDataTooLong)
	DESCENUM_DEF_VALUE(MetaDataCannotChangeLen)
	DESCENUM_DEF_VALUE(NameInvalid)
	DESCENUM_DEF_VALUE(NameNotInDir)
	DESCENUM_DEF_VALUE(NameExists)
	DESCENUM_DEF_VALUE(InvalidPathSyntax)
	DESCENUM_DEF_VALUE(MoveDestInvalid)
	DESCENUM_DEF_VALUE(DirNotEmpty)
	DESCENUM_DEF_VALUE(FileNotEmpty)
	DESCENUM_DEF_VALUE(InvalidOffset)
	DESCENUM_DEF_CLOSE();



	// AfsBlock

	void AfsBlock::Init(AfsStorage& storage, uint64 blockIndex, Rp<RcBlock> const& dataBlock) noexcept
	{
		EnsureThrow(!m_storage);
		m_storage = &storage;
		m_blockSize = storage.BlockSize();
		m_blockIndex = blockIndex;
		m_dataBlock = dataBlock;
	}


	void AfsBlock::SetChangePending()
	{
		EnsureThrow(!ChangePending());
		EnsureThrow(nullptr != m_changeTracker);

		m_dataBlockOrig.Set(std::move(m_dataBlock));
		m_dataBlock = new RcBlock { m_storage->Allocator() };
		Mem::Copy<byte>(m_dataBlock->Ptr(), m_dataBlockOrig->Ptr(), m_blockSize);

		m_changeTracker->AddChangedBlock(this);
	}


	void AfsBlock::OnWriteComplete()
	{
		EnsureThrow(ChangePending());
		m_dataBlockOrig.Clear();
		m_changeTracker = nullptr;
	}


	void AfsBlock::OnWriteAborted()
	{
		EnsureThrow(ChangePending());
		EnsureThrow(m_dataBlock.Any());

		m_dataBlock.Set(std::move(m_dataBlockOrig));
		m_changeTracker = nullptr;
	}



	// Afs::JournaledWrite

	Afs::JournaledWrite::JournaledWrite(Afs& afs) : m_afs(afs)
	{
		m_afs.m_storage->BeginJournaledWrite();

		// Set change trackers after BeginJournaledWrite in case it throws an exception
		m_afs.m_rootDirTopNode->SetChangeTracker(*this);
		m_afs.m_masterBlock->SetChangeTracker(*this);
		m_afs.m_freeListTailBlock->SetChangeTracker(*this);
	}


	Afs::JournaledWrite::~JournaledWrite()
	{
		if (!m_completed)
		{
			m_afs.m_storage->AbortJournaledWrite();
			for (Rp<AfsBlock> const& block : m_changedBlocks)
				block->OnWriteAborted();
		}

		m_afs.m_masterBlock->ClearChangeTracker();
		m_afs.m_freeListTailBlock->ClearChangeTracker();
		m_afs.m_rootDirTopNode->ClearChangeTracker();
	}


	void Afs::JournaledWrite::AddBlockToFree(Rp<VarBlock> const& block)
	{
		Mem::Zero<byte>(block->WritePtr(), m_afs.m_blockSize);
		m_blocksToFree.Add(block);
	}


	Rp<Afs::VarBlock> Afs::JournaledWrite::ReclaimBlockOrAddNew(BlockKind::E blockKind)
	{
		Rp<VarBlock> block = TryReclaimBlock();
		if (!block.Any())
		{
			block = new VarBlock { this };
			AfsResult::E r = m_afs.m_storage->AddNewBlock(block.Ref());
			switch (r)
			{
			case AfsResult::StorageInErrorState:
			case AfsResult::OutOfSpace:
				throw r;
			default:
				EnsureThrow(r == AfsResult::OK);
			}
		}
		block->SetBlockKind() = blockKind;
		return block;
	}


	Rp<Afs::VarBlock> Afs::JournaledWrite::TryReclaimBlock()
	{
		EnsureThrow(!m_completed);

		// Try to reclaim block from this journaled write
		if (m_blocksToFree.Any())
		{
			Rp<VarBlock> block = m_blocksToFree.Last();
			m_blocksToFree.PopLast();
			return block;
		}

		// Try to reclaim block from free list tail block
		FreeListView freeList = GetNewFreeListTailBlock().AsFreeListView();
		uint32 nrIndices = freeList.GetNrIndices();
		if (0 != nrIndices)
		{
			uint64 blockIndex = freeList.GetFreeBlockIndex(nrIndices - 1);
			--(freeList.SetNrIndices());

			Rp<VarBlock> block = new VarBlock { this };
			AfsResult::E r = m_afs.m_storage->ObtainBlock(block.Ref(), blockIndex);
			EnsureThrow(AfsResult::OK == r);
			EnsureThrowWithNr(block->GetBlockKind() == BlockKind::FreeBlock, block->GetBlockKind());
			return block;
		}

		// Try to reclaim the actual free list tail block, replacing it with previous free list block
		uint64 prevFreeListBlockIndex = freeList.GetPrevFreeListBlockIndex();
		if (UINT64_MAX != prevFreeListBlockIndex)
		{
			Rp<VarBlock> block = std::move(m_newFreeListTailBlock);

			m_newFreeListTailBlock = new VarBlock { this };
			AfsResult::E r = m_afs.m_storage->ObtainBlock(m_newFreeListTailBlock.Ref(), prevFreeListBlockIndex);
			EnsureThrow(AfsResult::OK == r);
			EnsureThrowWithNr(m_newFreeListTailBlock->GetBlockKind() == BlockKind::FreeList, block->GetBlockKind());

			FreeListView freeListNew = m_newFreeListTailBlock->AsFreeListView();
			EnsureThrow(freeListNew.GetNrIndices() == freeListNew.MaxNrIndices(m_afs.m_blockSize));

			EnsureThrow(0 != m_afs.m_masterBlock->GetNrFullFreeListNodes());
			m_afs.m_masterBlock->SetFreeListTailBlockIndex() = m_newFreeListTailBlock->BlockIndex();
			--(m_afs.m_masterBlock->SetNrFullFreeListNodes());

			return block;
		}
		
		return nullptr;
	}


	Afs::VarBlock& Afs::JournaledWrite::GetNewFreeListTailBlock()
	{
		if (!m_newFreeListTailBlock.Any())
			m_newFreeListTailBlock = m_afs.m_freeListTailBlock;

		return m_newFreeListTailBlock.Ref();
	}


	void Afs::JournaledWrite::Complete()
	{
		EnsureThrow(!m_completed);
		EnsureThrow(0 == m_nrFinalizationsPending);

		if (m_blocksToFree.Any())
		{
			uint32 const maxNrIndices = GetNewFreeListTailBlock().AsFreeListView().MaxNrIndices(m_afs.m_blockSize);
			for (Rp<VarBlock> const& block : m_blocksToFree)
			{
				FreeListView freeList = GetNewFreeListTailBlock().AsFreeListView();
				uint32 nrIndices = freeList.GetNrIndices();
				if (maxNrIndices != nrIndices)
				{
					// The current free list tail block has room. This block is added to the free list tail block
					EnsureThrowWithNr2(maxNrIndices > nrIndices, maxNrIndices, nrIndices);
					freeList.SetFreeBlockIndex(nrIndices) = block->BlockIndex();
					++(freeList.SetNrIndices());

					// The block becomes a typed block of kind BlockKind::FreeBlock
					block->SetBlockKind() = BlockKind::FreeBlock;
				}
				else
				{
					// The current free list tail block has no more room. This block becomes the new free list tail block
					Rp<VarBlock> prevTailBlock = m_newFreeListTailBlock;
					m_newFreeListTailBlock = block;
					m_newFreeListTailBlock->SetBlockKind() = BlockKind::FreeList;

					FreeListView freeListNew = m_newFreeListTailBlock->AsFreeListView();
					freeListNew.SetPrevFreeListBlockIndex() = prevTailBlock->BlockIndex();
					freeListNew.SetNrIndices() = 0;

					m_afs.m_masterBlock->SetFreeListTailBlockIndex() = m_newFreeListTailBlock->BlockIndex();
					++(m_afs.m_masterBlock->SetNrFullFreeListNodes());
				}
			}
		}

		m_afs.m_storage->CompleteJournaledWrite(m_changedBlocks);
		m_completed = true;

		if (m_newFreeListTailBlock.Any())
			if (m_afs.m_freeListTailBlock.Ptr() != m_newFreeListTailBlock.Ptr())
				m_afs.m_freeListTailBlock = std::move(m_newFreeListTailBlock);

		for (Rp<AfsBlock> const& block : m_changedBlocks)
			block->OnWriteComplete();
	}



	// Afs::BlockWithKind

	AfsResult::E Afs::BlockWithKind::ObtainBlock_CheckKind(AfsStorage* storage, uint64 blockIndex, BlockKind::E kind)
	{
		AfsResult::E r = storage->ObtainBlock(*this, blockIndex);
		if (AfsResult::OK != r) return r;
		if (GetBlockKind() != kind) return AfsResult::UnexpectedBlockKind;
		return AfsResult::OK;
	}



	// Afs::DirLeafView

	void Afs::DirLeafView::DecodeEntries(Vec<DirLeafEntry>& entries)
	{
		// DirLeafEntry::m_name is a Seq; therefore, must point to data which will not be overwritten during encoding
		EnsureThrow(!m_block.ChangePending());

		uint32 const nrEntries = GetNrEntries();
		Seq reader { ReadPtr() + FixedFieldsBytes, m_len - FixedFieldsBytes };
		uint nameLen;

		for (sizet i=0; i!=nrEntries; ++i)
		{
			DirLeafEntry& e = entries.Add();
			EnsureThrow(e.m_id.DecodeBin(reader));
			e.m_type = (ObjType::E) reader.ReadByte();
			EnsureThrow(DecodeUInt16LE(reader, nameLen));
			e.m_name = reader.ReadBytes(nameLen);
			EnsureThrowWithNr2(e.m_name.n == nameLen, e.m_name.n, nameLen);
		}
	}


	void Afs::DirLeafView::EncodeEntries(Vec<DirLeafEntry> const& entries)
	{
		SetNrEntries() = NumCast<uint32>(entries.Len());
		byte* p = WritePtr() + FixedFieldsBytes;
		byte const* pEnd = WritePtr() + m_len;

		for (DirLeafEntry const& e : entries)
		{
			uint16 nameLen = NumCast<uint16>(e.m_name.n);
			EnsureThrow(pEnd >= p + DirLeafEntry::EncodedSizeOverhead + nameLen);
			p = e.m_id.EncodeBin_Ptr(p);
			*p++ = e.m_type;
			p = EncodeUInt16LE_Ptr(p, nameLen);
			Mem::Copy<byte>(p, e.m_name.p, nameLen);
			p += nameLen;
		}

		EnsureAbortWithNr2(pEnd >= p, (ptrdiff) pEnd, (ptrdiff) p);
	}


	uint32 Afs::DirLeafView::EncodedSizeEntries(Vec<DirLeafEntry> const& entries) const
	{
		sizet n {};
		for (DirLeafEntry const& e : entries)
			n += e.EncodedSize();
		return NumCast<uint32>(n);
	}



	// Afs::DirBranchView

	void Afs::DirBranchView::DecodeEntries(Vec<DirBranchEntry>& entries)
	{
		// DirBranchEntry::m_name is a Seq; therefore, must point to data which will not be overwritten during encoding
		EnsureThrow(!m_block.ChangePending());

		uint32 const nrEntries = GetNrEntries();
		Seq reader { ReadPtr() + FixedFieldsBytes, m_len - FixedFieldsBytes };
		uint nameLen;

		for (sizet i=0; i!=nrEntries; ++i)
		{
			DirBranchEntry& e = entries.Add();
			EnsureThrow(DecodeUInt64LE(reader, e.m_blockIndex));
			EnsureThrow(DecodeUInt16LE(reader, nameLen));
			e.m_name = reader.ReadBytes(nameLen);
			EnsureThrowWithNr2(e.m_name.n == nameLen, e.m_name.n, nameLen);
		}
	}


	void Afs::DirBranchView::EncodeEntries(Vec<DirBranchEntry> const& entries)
	{
		SetNrEntries() = NumCast<uint32>(entries.Len());
		byte* p = WritePtr() + FixedFieldsBytes;
		byte const* pEnd = WritePtr() + m_len;

		for (DirBranchEntry const& e : entries)
		{
			uint16 nameLen = NumCast<uint16>(e.m_name.n);
			EnsureThrow(pEnd >= p + DirBranchEntry::EncodedSizeOverhead + nameLen);
			p = EncodeUInt64LE_Ptr(p, e.m_blockIndex);
			p = EncodeUInt16LE_Ptr(p, nameLen);
			Mem::Copy<byte>(p, e.m_name.p, nameLen);
			p += nameLen;
		}

		EnsureAbortWithNr2(pEnd >= p, (ptrdiff) pEnd, (ptrdiff) p);
	}


	uint32 Afs::DirBranchView::EncodedSizeEntries(Vec<DirBranchEntry> const& entries) const
	{
		sizet n {};
		for (DirBranchEntry const& e : entries)
			n += e.EncodedSize();
		return NumCast<uint32>(n);
	}



	// Afs::FileLeafView

	void Afs::FileLeafView::DecodeEntries(Vec<FileLeafEntry>& entries)
	{
		uint32 const nrEntries = GetNrEntries();
		Seq reader { ReadPtr() + FixedFieldsBytes, m_len - FixedFieldsBytes };

		for (sizet i=0; i!=nrEntries; ++i)
		{
			FileLeafEntry& e = entries.Add();
			EnsureThrow(DecodeUInt64LE(reader, e.m_blockIndex));
		}
	}


	void Afs::FileLeafView::EncodeEntries(Vec<FileLeafEntry> const& entries)
	{
		SetNrEntries() = NumCast<uint32>(entries.Len());
		byte* p = WritePtr() + FixedFieldsBytes;
		byte const* pEnd = WritePtr() + m_len;
		sizet entriesEncodedSize = entries.Len() * FileLeafEntry::EncodedSize;
		EnsureThrow(pEnd >= p + entriesEncodedSize);

		for (FileLeafEntry const& e : entries)
			p = EncodeUInt64LE_Ptr(p, e.m_blockIndex);

		EnsureAbortWithNr2(pEnd >= p, (ptrdiff) pEnd, (ptrdiff) p);
	}


	uint32 Afs::FileLeafView::EncodedSizeEntries(Vec<FileLeafEntry> const& entries) const
	{
		sizet n = entries.Len() * FileLeafEntry::EncodedSize;
		return NumCast<uint32>(n);
	}



	// Afs::FileBranchView

	void Afs::FileBranchView::DecodeEntries(Vec<FileBranchEntry>& entries)
	{
		uint32 const nrEntries = GetNrEntries();
		Seq reader { ReadPtr() + FixedFieldsBytes, m_len - FixedFieldsBytes };

		for (sizet i=0; i!=nrEntries; ++i)
		{
			FileBranchEntry& e = entries.Add();
			EnsureThrow(DecodeUInt64LE(reader, e.m_fileOffset));
			EnsureThrow(DecodeUInt64LE(reader, e.m_blockIndex));
		}
	}


	void Afs::FileBranchView::EncodeEntries(Vec<FileBranchEntry> const& entries)
	{
		SetNrEntries() = NumCast<uint32>(entries.Len());
		byte* p = WritePtr() + FixedFieldsBytes;
		byte const* pEnd = WritePtr() + m_len;
		EnsureThrow(pEnd >= p + (entries.Len() * FileBranchEntry::EncodedSize));

		for (FileBranchEntry const& e : entries)
		{
			p = EncodeUInt64LE_Ptr(p, e.m_fileOffset);
			p = EncodeUInt64LE_Ptr(p, e.m_blockIndex);
		}

		EnsureAbortWithNr2(pEnd >= p, (ptrdiff) pEnd, (ptrdiff) p);
	}


	uint32 Afs::FileBranchView::EncodedSizeEntries(Vec<FileBranchEntry> const& entries) const
	{
		sizet n = entries.Len() * FileBranchEntry::EncodedSize;
		return NumCast<uint32>(n);
	}



	// Afs::DirNode

	void Afs::DirNode::Decode()
	{
		NodeView node = m_block->AsNodeView();
		m_isTop = (node.GetNodeCat() == NodeCat::Top);

		DirView dir = node.AsAnyDirView();
		m_level = dir.GetDirNodeLevel();
		if (!m_level)
			dir.AsLeafView().DecodeEntries(m_leafEntries);
		else
		{
			dir.AsBranchView().DecodeEntries(m_branchEntries);
			m_childNodes.Clear().ResizeExact(m_branchEntries.Len());
		}
	}


	void Afs::DirNode::Encode()
	{
		NodeView node = m_block->AsNodeView();
		EnsureThrow((node.GetNodeCat() == NodeCat::Top) == m_isTop);

		DirView dir = node.AsAnyDirView();
		EnsureThrowWithNr2(dir.GetDirNodeLevel() == (byte) m_level, dir.GetDirNodeLevel(), m_level);

		if (!m_level)
		{
			EnsureThrowWithNr(0 == m_branchEntries.Len(), m_branchEntries.Len());
			EnsureThrowWithNr(0 == m_childNodes.Len(), m_childNodes.Len());
			dir.AsLeafView().EncodeEntries(m_leafEntries);
		}
		else
		{
			EnsureThrowWithNr(0 == m_leafEntries.Len(), m_leafEntries.Len());
			EnsureThrowWithNr2(m_branchEntries.Len() == m_childNodes.Len(), m_branchEntries.Len(), m_childNodes.Len());
			dir.AsBranchView().EncodeEntries(m_branchEntries);
		}
	}


	uint32 Afs::DirNode::AsIf_EncodedSizeOverhead(uint32 level)
	{
		DirView dir = m_block->AsNodeView().AsAnyDirView();
		if (!level)
			return dir.AsIf_LeafView().EncodedSizeOverhead();
		else
			return dir.AsIf_BranchView().EncodedSizeOverhead();
	}


	uint32 Afs::DirNode::EncodedSizeEntries()
	{
		DirView dir = m_block->AsNodeView().AsAnyDirView();
		if (!m_level)
			return dir.AsLeafView().EncodedSizeEntries(m_leafEntries);
		else
			return dir.AsBranchView().EncodedSizeEntries(m_branchEntries);
	}



	// Afs::CxBase

	void Afs::CxBase::ChangeState(Node& node, NodeState newState)
	{
		EnsureThrowWithNr2(node.m_state <= newState, node.m_state, newState);
		node.m_state = newState;

		if (!m_anyChanged)
		{
			EnsureThrow(nullptr != m_jw);
			m_jw->IncrementFinalizationsPending();
			m_anyChanged = true;
		}
	}



	// Afs::DirCxR

	AfsResult::E Afs::DirCxR::GetDirTopBlock(ObjId id)
	{
		EnsureThrow(nullptr == m_topNode);
		m_nodes.ReserveInc(1);
		m_topNode = (m_nodes.Add() = new DirNode).Ptr();
		m_topNode->m_block = new VarBlock { m_jw };

		AfsResult::E r = m_afs.GetTopBlock(m_topNode->m_block.Ref(), id, ObjType::Dir);
		if (AfsResult::OK != r)
			return r;

		m_topNode->Decode();
		return AfsResult::OK;
	}


	Afs::FindResult Afs::DirCxR::NavToLeafEntryEqualOrLessThan(DirNavPath& navPath, Seq name, StopEarly stopEarly)
	{
		navPath.Clear();
		navPath.Add().m_node = m_topNode;

		while (true)
		{
			DirNavEntry& navEntry = navPath.Last();

			if (!navEntry.m_node->m_level)
			{
				FindResult fr = m_afs.FindNameEqualOrLessThan(navEntry.m_node->m_leafEntries, name, navEntry.m_pos);
				if (FindResult::NoEntries == fr)
					EnsureThrow(navEntry.m_node == m_topNode);		// A leaf node may be empty only if it is the top node
				return fr;
			}
			else
			{
				FindResult fr = m_afs.FindNameEqualOrLessThan(navEntry.m_node->m_branchEntries, name, navEntry.m_pos);
				EnsureThrow(FindResult::NoEntries != fr);			// A branch node may not be empty

				if (StopEarly::IfCantFind == stopEarly && FindResult::FirstIsGreater == fr)
					return fr;										// Stop early if name cannot be in directory

				DescendToNextChildNode(navPath, EnumDir::Forward);
			}
		}
	}


	Afs::DirLeafEntry& Afs::DirCxR::GetLeafEntryAt(DirNavPath& navPath)
	{
		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;

		EnsureThrow(!node.m_level);
		EnsureThrow(navEntry.m_pos < node.m_leafEntries.Len());
		return node.m_leafEntries[navEntry.m_pos];
	}


	bool Afs::DirCxR::NavToSiblingNode(DirNavPath& navPath, EnumDir enumDir)
	{
		// Get current node level
		uint32 targetLevel;

		{
			DirNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			targetLevel = navEntry.m_node->m_level;
		}

		// Ascend until we find a higher level node which allows us to move in the desired direction
		uint32 childLevel = targetLevel;
		while (true)
		{
			uint64 childBlockIndex = navPath.Last().m_node->m_block->BlockIndex();
			navPath.PopLast();

			// We have exhausted higher levels and still cannot move in the desired direction. Return with "navPath" cleared
			if (!navPath.Any())
				return false;

			// Verify that the current parent position corresponds to child from which we ascended
			DirNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			DirNode& node = *navEntry.m_node;
			EnsureThrowWithNr2(childLevel + 1 == node.m_level, childLevel, node.m_level);
			EnsureThrowWithNr2(node.m_branchEntries.Len() > navEntry.m_pos, node.m_branchEntries.Len(), navEntry.m_pos);
			DirBranchEntry& branchEntry = node.m_branchEntries[navEntry.m_pos];
			EnsureThrowWithNr2(branchEntry.m_blockIndex == childBlockIndex, branchEntry.m_blockIndex, childBlockIndex);

			// Can we move in the desired lateral direction?
			if (EnumDir::Forward == enumDir)
			{
				if (navEntry.m_pos + 1 < node.m_branchEntries.Len())
				{
					++(navEntry.m_pos);
					break;
				}
			}
			else if (EnumDir::Reverse == enumDir)
			{
				if (navEntry.m_pos > 0)
				{
					--(navEntry.m_pos);
					break;
				}
			}
			else
				EnsureThrow(!"Unrecognized EnumDir");

			// We can't yet move in the desired lateral direction. Keep going up
			++childLevel;
		}

		// Descend to sibling at target level
		while (true)
		{
			DescendToNextChildNode(navPath, enumDir);

			DirNavEntry& navEntry = navPath.Last();
			DirNode& node = *navEntry.m_node;
			if (node.m_level == targetLevel)
				break;

			EnsureThrow(0 != node.m_level);
			EnsureThrow(node.m_branchEntries.Any());
		}

		return true;
	}


	void Afs::DirCxR::DescendToNextChildNode(DirNavPath& navPath, EnumDir enumDir)
	{
		EnsureThrow(navPath.Any());

		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		EnsureThrow(0 != node.m_level);
		EnsureThrowWithNr2(node.m_childNodes.Len() > navEntry.m_pos, node.m_childNodes.Len(), navEntry.m_pos);

		DirNode*& pChildNode = node.m_childNodes[navEntry.m_pos];
		if (!pChildNode)
		{
			m_nodes.ReserveInc(1);
			pChildNode = (m_nodes.Add() = new DirNode).Ptr();

			DirNode& childNode = *pChildNode;
			EnsureThrowWithNr2(node.m_branchEntries.Len() > navEntry.m_pos, node.m_branchEntries.Len(), navEntry.m_pos);
			DirBranchEntry& branchEntry = node.m_branchEntries[navEntry.m_pos];
			childNode.m_block = new VarBlock { m_jw };
			AfsResult::E r = childNode.m_block->ObtainBlock_CheckKind(m_afs.m_storage, branchEntry.m_blockIndex, BlockKind::Node);
			EnsureThrowWithNr(AfsResult::OK == r, r);
			childNode.Decode();
			EnsureThrowWithNr2(childNode.m_level + 1 == node.m_level, childNode.m_level, node.m_level);
		}
				
		navPath.Add().m_node = pChildNode;

		if (EnumDir::Reverse == enumDir)
		{
			sizet const nrEntries = pChildNode->NrVecEntries();
			EnsureThrow(0 != nrEntries);
			navPath.Last().m_pos = nrEntries - 1;
		}
	}



	// Afs::DirCxRW

	void Afs::DirCxRW::RemoveLeafEntryAt(DirNavPath& navPath, ObjId leafEntryId, Time now)
	{
		// Verify that m_navPath points to the expected leaf entry
		EnsureThrow(navPath.Any());
		DirNavEntry const& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		EnsureThrow(!node.m_level);
		EnsureThrow(node.m_leafEntries.Len() > navEntry.m_pos);
		DirLeafEntry const& dirLeafEntry = node.m_leafEntries[navEntry.m_pos];
		EnsureThrow(dirLeafEntry.m_id == leafEntryId);

		// Remove leaf entry
		node.m_leafEntries.Erase(navEntry.m_pos, 1);
		ChangeState(node, NodeState::Changed);

		OnEntryRemoved_Maintenance(navPath);

		// Update top node
		TopView top = m_topNode->m_block->AsNodeView().AsTopView();
		EnsureThrow(0 != top.Get_Dir_NrEntries());
		--(top.Set_Dir_NrEntries());
		top.SetModifyTime() = now.ToFt();
		ChangeState(*m_topNode, NodeState::Changed);
	}


	void Afs::DirCxRW::AddLeafEntry(DirLeafEntry const& entry, Time now)
	{
		DirNavPath navPath;
		FindResult fr = NavToLeafEntryEqualOrLessThan(navPath, entry.m_name, StopEarly::No);
		EnsureThrow(FindResult::FoundEqual != fr);
		if (FindResult::FoundLessThan == fr)
			++(navPath.Last().m_pos);
		AddLeafEntryAt(entry, navPath, now, CanAddNode::Yes);
	}


	void Afs::DirCxRW::AddLeafEntryAt(DirLeafEntry const& entry, DirNavPath& navPath, Time now, CanAddNode canAddNode)
	{
		uint32 const entryEncodedSize = entry.EncodedSize();

		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		EnsureThrowWithNr(0 == node.m_level, node.m_level);

		if (node.EncodedSize() + entryEncodedSize <= m_afs.m_blockSize)
		{
			// The entry can be inserted into this node
			node.m_leafEntries.Insert(navEntry.m_pos, entry);
			if (0 < navEntry.m_pos)
				EnsureThrow(0 > m_afs.m_cmp(node.m_leafEntries[navEntry.m_pos-1].m_name, node.m_leafEntries[navEntry.m_pos].m_name));
			if (node.m_leafEntries.Len() > navEntry.m_pos + 1)
				EnsureThrow(0 > m_afs.m_cmp(node.m_leafEntries[navEntry.m_pos].m_name, node.m_leafEntries[navEntry.m_pos+1].m_name));
			ChangeState(node, NodeState::Changed);

			if (!node.m_isTop && 0 == navEntry.m_pos)
				UpdateAncestors(navPath);

			// Update top node
			TopView top = m_topNode->m_block->AsNodeView().AsTopView();
			++(top.Set_Dir_NrEntries());
			top.SetModifyTime() = now.ToFt();
			ChangeState(*m_topNode, NodeState::Changed);
		}
		else
		{
			// Insufficient room to insert entry. Node block must split
			EnsureThrow(CanAddNode::Yes == canAddNode);
			SplitNode(navPath);
			AddLeafEntryAt(entry, navPath, now, CanAddNode::No);
		}
	}


	void Afs::DirCxRW::AddBranchEntryAt(DirBranchEntry& entry, DirNavPath& navPath, DirNode* newNode, CanAddNode canAddNode)
	{
		uint32 const entryEncodedSize = entry.EncodedSize();

		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		EnsureThrow(0 != node.m_level);

		if (node.EncodedSize() + entryEncodedSize <= m_afs.m_blockSize)
		{
			// The entry can be inserted into this node
			EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());
			node.m_branchEntries.Insert(navEntry.m_pos, entry);
			node.m_childNodes.Insert(navEntry.m_pos, newNode);

			if (0 < navEntry.m_pos)
				EnsureThrow(0 > m_afs.m_cmp(node.m_branchEntries[navEntry.m_pos-1].m_name, node.m_branchEntries[navEntry.m_pos].m_name));
			if (node.m_leafEntries.Len() > navEntry.m_pos + 1)
				EnsureThrow(0 > m_afs.m_cmp(node.m_branchEntries[navEntry.m_pos].m_name, node.m_branchEntries[navEntry.m_pos+1].m_name));
			ChangeState(node, NodeState::Changed);

			if (!node.m_isTop && 0 == navEntry.m_pos)
				UpdateAncestors(navPath);
		}
		else
		{
			// Insufficient room to insert entry. Node block must split
			EnsureThrow(CanAddNode::Yes == canAddNode);
			SplitNode(navPath);
			AddBranchEntryAt(entry, navPath, newNode, CanAddNode::No);
		}
	}


	void Afs::DirCxRW::Finalize()
	{
		if (!m_anyChanged)
			return;

		m_jw->DecrementFinalizationsPending();

		for (Rp<DirNode> const& node : m_nodes)
		{
			if (node->m_state == NodeState::Changed)
			{
				node->Encode();
				node->m_state = NodeState::Finalized;
			}
			else if (node->m_state == NodeState::Free)
			{
				m_jw->AddBlockToFree(node->m_block);
				node->m_state = NodeState::Finalized;
			}
		}
	}


	void Afs::DirCxRW::RemoveNonTopNode(DirNavPath& navPath)
	{
		uint64 blockIndex;
		uint32 removedNodeLevel;

		// Check the node being removed; get its block index; schedule it to be added to the free list
		{
			EnsureThrowWithNr(navPath.Len() >= 2, navPath.Len());
			DirNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			DirNode& node = *navEntry.m_node;
			EnsureThrow(!node.m_isTop);

			blockIndex = node.m_block->BlockIndex();
			removedNodeLevel = node.m_level;

			node.m_state = NodeState::Free;
		}

		// In the parent node, remove the DirBranchEntry that points to the removed block
		{
			navPath.PopLast();

			DirNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			DirNode& node = *navEntry.m_node;
			EnsureThrowWithNr2(removedNodeLevel + 1 == node.m_level, removedNodeLevel, node.m_level);
			EnsureThrowWithNr2(node.m_branchEntries.Len() > navEntry.m_pos, node.m_branchEntries.Len(), navEntry.m_pos);
			DirBranchEntry& branchEntry = node.m_branchEntries[navEntry.m_pos];
			EnsureThrowWithNr2(branchEntry.m_blockIndex == blockIndex, branchEntry.m_blockIndex, blockIndex);

			node.m_branchEntries.Erase(navEntry.m_pos, 1);
			node.m_childNodes.Erase(navEntry.m_pos, 1);
			ChangeState(node, NodeState::Changed);

			OnEntryRemoved_Maintenance(navPath);
		}
	}


	void Afs::DirCxRW::OnEntryRemoved_Maintenance(DirNavPath& navPath)
	{
		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;

		if (node.m_isTop)
		{
			sizet const nrEntries = node.NrVecEntries();
			if (0 == nrEntries)
				EnsureThrowWithNr(0 == node.m_level, node.m_level);		// If the top node now has no remaining entries, it must be a leaf node
			else if (1 == nrEntries && 0 != node.m_level)
				TryHoistIntoTopNode(navPath);

			return;
		}

		if (2 == navPath.Len() && 1 == m_topNode->m_branchEntries.Len())
		{
			if (TryHoistIntoTopNode(navPath))
				return;
		}

		if (!node.AnyVecEntries())
		{
			// The node is now empty. Since it is non-top, it can and must be removed
			// This can cause cascading updates to higher level nodes, including joining and freeing
			RemoveNonTopNode(navPath);
			return;
		}

		if (0 == navEntry.m_pos)
			UpdateAncestors(navPath);

		if (node.EncodedSize() <= m_afs.m_blockSize / NodeRebalanceThresholdFraction)
		{
			// Encoded size of the node's entries falls below a rebalance threshold. Check neighboring nodes for rebalance opportunities
			uint32 encodedSizeWithPrev = UINT32_MAX, encodedSizeWithNext = UINT32_MAX;
			uint32 encodedSizeEntries = node.EncodedSizeEntries();

			DirNavPath navPathPrev { navPath };
			if (NavToSiblingNode(navPathPrev, EnumDir::Reverse))
			{
				DirNode* prevNode = navPathPrev.Last().m_node;
				EnsureThrow(nullptr != prevNode);
				encodedSizeWithPrev = prevNode->EncodedSize() + encodedSizeEntries;
			}
				
			DirNavPath navPathNext { navPath };
			if (NavToSiblingNode(navPathNext, EnumDir::Forward))
			{
				DirNode* nextNode = navPathNext.Last().m_node;
				EnsureThrow(nullptr != nextNode);
				encodedSizeWithNext = nextNode->EncodedSize() + encodedSizeEntries;
			}

			if (encodedSizeWithPrev <= encodedSizeWithNext)
			{
				if (encodedSizeWithPrev <= m_afs.m_blockSize)
					JoinSiblingNodes(navPathPrev, navPath);
			}
			else
			{
				if (encodedSizeWithNext <= m_afs.m_blockSize)
					JoinSiblingNodes(navPath, navPathNext);
			}
		}
	}


	void Afs::DirCxRW::UpdateAncestors(DirNavPath& navPath)
	{
		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		EnsureThrow(!node.m_isTop);

		if (!node.NrVecEntries())
			return;		// Node will be removed in maintenance

		Seq newFirstName = node.FirstName();

		EnsureThrowWithNr(2 <= navPath.Len(), navPath.Len());
		sizet navIndex = navPath.Len() - 2;
		uint32 childLevel = node.m_level;
		uint64 childBlockIndex = node.m_block->BlockIndex();
		VecFix<DirNode*, NavPath_MaxEntries> splitNodes;

		while (true)
		{
			DirNavEntry* ancestorNav = &navPath[navIndex];
			EnsureThrow(nullptr != ancestorNav->m_node);
			DirNode* ancestorNode = ancestorNav->m_node;
			EnsureThrowWithNr2(childLevel + 1 == ancestorNode->m_level, childLevel, ancestorNode->m_level);
			EnsureThrowWithNr2(ancestorNode->m_branchEntries.Len() > ancestorNav->m_pos, ancestorNode->m_branchEntries.Len(), ancestorNav->m_pos);
			DirBranchEntry& branchEntry = ancestorNode->m_branchEntries[ancestorNav->m_pos];
			EnsureThrowWithNr2(branchEntry.m_blockIndex == childBlockIndex, branchEntry.m_blockIndex, childBlockIndex);

			if (0 == m_afs.m_cmp(branchEntry.m_name, newFirstName))
				break;

			if (0 != ancestorNav->m_pos)
			{
				Seq prevName = ancestorNode->m_branchEntries[ancestorNav->m_pos - 1].m_name;
				EnsureThrow(0 > m_afs.m_cmp(prevName, newFirstName));
			}

			if (ancestorNav->m_pos + 1 < ancestorNode->m_branchEntries.Len())
			{
				Seq nextName = ancestorNode->m_branchEntries[ancestorNav->m_pos + 1].m_name;
				EnsureThrow(0 > m_afs.m_cmp(newFirstName, nextName));
			}

			bool newNameLonger {};
			if (newFirstName.n > branchEntry.m_name.n)
				newNameLonger = true;

			branchEntry.m_name = newFirstName;
			ChangeState(*ancestorNode, NodeState::Changed);

			if (newNameLonger)
				if (ancestorNode->EncodedSize() > m_afs.m_blockSize)
					splitNodes.Add(ancestorNode);

			if (0 == navIndex) break;
			if (0 != ancestorNav->m_pos) break;

			childLevel = ancestorNode->m_level;
			childBlockIndex = ancestorNode->m_block->BlockIndex();
			--navIndex;
		}

		// Cannot split until we have updated ancestors, otherwise we can't rebuild NavPaths
		if (splitNodes.Any())
		{
			do
			{
				DirNode* splitNode = splitNodes.Last();
				splitNodes.PopLast();

				// Node may have already been split while splitting another node, so recheck if split is still necessary
				if (splitNode->EncodedSize() > m_afs.m_blockSize)
				{
					DirNavPath splitNavPath;
					RebuildNavPath(splitNavPath, *splitNode, 0);
					SplitNode(splitNavPath);
				}
			}
			while (splitNodes.Any());

			RebuildNavPath(navPath, node, navEntry.m_pos);
		}
	}


	bool Afs::DirCxRW::TryHoistIntoTopNode(DirNavPath& navPath)
	{
		// When the top node has exactly one entry remaining, the child node (leaf or branch) needs to be hoisted into the top node.
		// However, since a top node contains extra information which a non-top node does not contain, the top node might not have
		// enough space to contain the entire child node when the number of top node entries is first reduced to 1.
		//
		// For this reason, we check to hoist the child node into the top node on two occasions:
		// - When the number of top node entries is first reduced to 1.
		// - When an immediate child node entry is removed, and the number of top node entries is already 1.
		//
		// The current implementation assumes the child node is not empty when hoisted. This requires a name length limit for
		// directory entries which is low enough relative to block size so that when entries are removed from a child node,
		// the child node can always be hoisted into the top node before it becomes empty.

		EnsureThrow(navPath.Any());
		DirNavEntry& topNavEntry = navPath.First();
		EnsureThrow(nullptr != topNavEntry.m_node);
		DirNode& topNode = *topNavEntry.m_node;
		EnsureThrow(0 != topNode.m_level);
		EnsureThrowWithNr(1 == topNode.m_branchEntries.Len(), topNode.m_branchEntries.Len());

		// If navPath does not point to the top node itself, it must already point to the child node to hoist
		bool pathDescended {};
		if (1 == navPath.Len())
		{
			topNavEntry.m_pos = 0;
			DescendToNextChildNode(navPath, EnumDir::Forward);
			pathDescended = true;
		}

		EnsureThrowWithNr(2 == navPath.Len(), navPath.Len());
		DirNavEntry& childNavEntry = navPath.Last();
		EnsureThrow(nullptr != childNavEntry.m_node);
		DirNode& childNode = *childNavEntry.m_node;

		if (topNode.AsIf_EncodedSizeOverhead(childNode.m_level) + childNode.EncodedSizeEntries() > m_afs.m_blockSize)
		{
			if (pathDescended)
				navPath.PopLast();
			return false;
		}

		// Child node does fit into top node. Hoist it
		DirView dirView = topNode.m_block->AsNodeView().AsTopView().AsDirView();
		dirView.SetDirNodeLevel() = (byte) childNode.m_level;
		topNode.m_level = childNode.m_level;

		if (0 == childNode.m_level)
		{
			topNode.m_leafEntries = std::move(childNode.m_leafEntries);
			topNode.m_branchEntries.Clear();
			topNode.m_childNodes.Clear();
		}
		else
		{
			topNode.m_branchEntries = std::move(childNode.m_branchEntries);
			topNode.m_childNodes = std::move(childNode.m_childNodes);
		}

		ChangeState(topNode, NodeState::Changed);
		ChangeState(childNode, NodeState::Free);

		topNavEntry.m_pos = childNavEntry.m_pos;
		navPath.PopLast();
		return true;
	}


	void Afs::DirCxRW::JoinSiblingNodes(DirNavPath& navPathTo, DirNavPath& navPathFrom)
	{
		DirNavEntry& navEntryTo = navPathTo.Last();
		EnsureThrow(nullptr != navEntryTo.m_node);
		DirNode& nodeTo = *navEntryTo.m_node;
		EnsureThrow(!nodeTo.m_isTop);

		DirNavEntry& navEntryFrom = navPathFrom.Last();
		EnsureThrow(nullptr != navEntryFrom.m_node);
		DirNode& nodeFrom = *navEntryFrom.m_node;
		EnsureThrow(!nodeFrom.m_isTop);

		EnsureThrowWithNr2(nodeTo.m_level == nodeFrom.m_level, nodeTo.m_level, nodeFrom.m_level);
		sizet newFromPos;

		if (0 == nodeTo.m_level)
		{
			EnsureThrow(nodeTo.m_leafEntries.Any());
			EnsureThrow(nodeFrom.m_leafEntries.Any());
			Seq lastToName = nodeTo.m_leafEntries.Last().m_name;
			Seq firstFromName = nodeFrom.m_leafEntries.First().m_name;
			EnsureThrow(0 > m_afs.m_cmp(lastToName, firstFromName));

			newFromPos = nodeTo.m_leafEntries.Len() + navEntryFrom.m_pos;
			nodeTo.m_leafEntries.MergeFrom(nodeFrom.m_leafEntries);
		}
		else
		{
			EnsureThrow(nodeTo.m_branchEntries.Any());
			EnsureThrow(nodeFrom.m_branchEntries.Any());
			Seq lastToName = nodeTo.m_branchEntries.Last().m_name;
			Seq firstFromName = nodeFrom.m_branchEntries.First().m_name;
			EnsureThrow(0 > m_afs.m_cmp(lastToName, firstFromName));

			newFromPos = nodeTo.m_branchEntries.Len() + navEntryFrom.m_pos;
			nodeTo.m_branchEntries.MergeFrom(nodeFrom.m_branchEntries);
			nodeTo.m_childNodes.MergeFrom(nodeFrom.m_childNodes);
		}

		EnsureThrow(nodeTo.EncodedSize() <= m_afs.m_blockSize);
		ChangeState(nodeTo, NodeState::Changed);

		RemoveNonTopNode(navPathFrom);
	}


	void Afs::DirCxRW::SplitNode(DirNavPath& navPath)
	{
		DirNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		DirNode& node = *navEntry.m_node;
		
		// Find best split position - middle of encoded data
		uint32 splitSizeThreshold = node.EncodedSizeEntries() / 2U;
		sizet sizeSum {};
		sizet splitIndex = SIZE_MAX;

		if (0 == node.m_level)
		{
			for (sizet i=0; i!=node.m_leafEntries.Len(); ++i)
			{
				sizeSum += node.m_leafEntries[i].EncodedSize();
				if (sizeSum >= splitSizeThreshold)
					{ splitIndex = i; break; }
			}
		}
		else
		{
			for (sizet i=0; i!=node.m_branchEntries.Len(); ++i)
			{
				sizeSum += node.m_branchEntries[i].EncodedSize();
				if (sizeSum >= splitSizeThreshold)
					{ splitIndex = i; break; }
			}
		}

		EnsureThrow(SIZE_MAX != splitIndex);

		if (node.m_isTop)
			SplitTopNode(navPath, node, splitIndex);
		else
			SplitNonTopNode(navPath, node, splitIndex);
	}


	void Afs::DirCxRW::SplitTopNode(DirNavPath& navPath, DirNode& node, sizet const splitIndex)
	{
		EnsureThrowWithNr(1 == navPath.Len(), navPath.Len());
		EnsureThrow(node.m_isTop);
		EnsureThrowWithNr(NodeLevel_BeyondMax > node.m_level, node.m_level);
		EnsureThrow(0 != splitIndex);

		m_nodes.ReserveInc(2);
		DirNode* child1 = (m_nodes.Add() = new DirNode).Ptr();
		DirNode* child2 = (m_nodes.Add() = new DirNode).Ptr();

		auto initChild = [this] (JournaledWrite* jw, DirNode& node, DirNode* child)
			{
				child->m_block = jw->ReclaimBlockOrAddNew(BlockKind::Node);
				child->m_level = node.m_level;
				ChangeState(*child, NodeState::Changed);

				NodeView nodeView = child->m_block->AsNodeView();
				nodeView.SetNodeCat() = NodeCat::NonTop;
				nodeView.SetObjType() = ObjType::Dir;

				DirView dirView = nodeView.AsNonTopDirView();
				dirView.SetDirNodeLevel() = (byte) node.m_level;
			};
 
		initChild(m_jw, node, child1);
		initChild(m_jw, node, child2);

		Seq child1FirstName, child2FirstName;

		if (0 == node.m_level)
		{
			EnsureThrow(!node.m_branchEntries.Any());
			EnsureThrow(!node.m_childNodes.Any());
			EnsureThrowWithNr2(splitIndex < node.m_leafEntries.Len(), splitIndex, node.m_leafEntries.Len());

			sizet const child2Entries = node.m_leafEntries.Len() - splitIndex;
			child1->m_leafEntries.ReserveExact(splitIndex);
			child2->m_leafEntries.ReserveExact(child2Entries);

			sizet i {};
			for (; i!=splitIndex; ++i)               child1->m_leafEntries.Add(std::move(node.m_leafEntries[i]));
			for (; i!=node.m_leafEntries.Len(); ++i) child2->m_leafEntries.Add(std::move(node.m_leafEntries[i]));

			child1FirstName = child1->m_leafEntries.First().m_name;
			child2FirstName = child2->m_leafEntries.First().m_name;
		}
		else
		{
			EnsureThrow(!node.m_leafEntries.Any());
			EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());
			EnsureThrowWithNr2(splitIndex + 1 < node.m_branchEntries.Len(), splitIndex, node.m_branchEntries.Len());

			sizet const child2Entries = node.m_branchEntries.Len() - splitIndex;
			child1->m_branchEntries.ReserveExact(splitIndex);
			child1->m_childNodes   .ReserveExact(splitIndex);
			child2->m_branchEntries.ReserveExact(child2Entries);
			child2->m_childNodes   .ReserveExact(child2Entries);

			sizet i {};
			for (; i!=splitIndex; ++i)                 { child1->m_branchEntries.Add(std::move(node.m_branchEntries[i])); child1->m_childNodes.Add(node.m_childNodes[i]); }
			for (; i!=node.m_branchEntries.Len(); ++i) { child2->m_branchEntries.Add(std::move(node.m_branchEntries[i])); child2->m_childNodes.Add(node.m_childNodes[i]); }

			child1FirstName = child1->m_branchEntries.First().m_name;
			child2FirstName = child2->m_branchEntries.First().m_name;
		}

		EnsureThrow(0 > m_afs.m_cmp(child1FirstName, child2FirstName));

		// Update top node
		++(node.m_level);
		EnsureThrow(NodeLevel_BeyondMax > node.m_level);

		DirView dirView = node.m_block->AsNodeView().AsTopView().AsDirView();
		dirView.SetDirNodeLevel() = (byte) node.m_level;

		node.m_leafEntries.Clear();
		
		node.m_branchEntries.ResizeExact(2);
		node.m_branchEntries[0].m_blockIndex = child1->m_block->BlockIndex();
		node.m_branchEntries[0].m_name = child1FirstName;
		node.m_branchEntries[1].m_blockIndex = child2->m_block->BlockIndex();
		node.m_branchEntries[1].m_name = child2FirstName;
		
		node.m_childNodes.ResizeExact(2);
		node.m_childNodes[0] = child1;
		node.m_childNodes[1] = child2;
		
		ChangeState(node, NodeState::Changed);

		// Update navPath
		DirNavEntry& navEntryOld = navPath.Last();
		sizet navPosOld = navEntryOld.m_pos;

		if (navPosOld < splitIndex)
		{
			navEntryOld.m_pos = 0;
			DirNavEntry& navEntryNew = navPath.Add();
			navEntryNew.m_node = child1;
			navEntryNew.m_pos = navPosOld;
		}
		else
		{
			navEntryOld.m_pos = 1;
			DirNavEntry& navEntryNew = navPath.Add();
			navEntryNew.m_node = child2;
			navEntryNew.m_pos = navPosOld - splitIndex;
		}
	}


	void Afs::DirCxRW::SplitNonTopNode(DirNavPath& navPath, DirNode& node, sizet splitIndex)
	{
		EnsureThrow(!node.m_isTop);
		EnsureThrow(0 != splitIndex);

		DirNode* newNode = (m_nodes.Add() = new DirNode).Ptr();
		newNode->m_block = m_jw->ReclaimBlockOrAddNew(BlockKind::Node);
		newNode->m_level = node.m_level;
		ChangeState(*newNode, NodeState::Changed);

		NodeView newNodeView = newNode->m_block->AsNodeView();
		newNodeView.SetNodeCat() = NodeCat::NonTop;
		newNodeView.SetObjType() = ObjType::Dir;

		DirView dirView = newNodeView.AsNonTopDirView();
		dirView.SetDirNodeLevel() = (byte) node.m_level;

		Seq newNodeFirstName;

		if (0 == node.m_level)
		{
			EnsureThrow(!node.m_branchEntries.Any());
			EnsureThrow(!node.m_childNodes.Any());
			EnsureThrowWithNr2(splitIndex < node.m_leafEntries.Len(), splitIndex, node.m_leafEntries.Len());
			
			sizet const newNodeEntries = node.m_leafEntries.Len() - splitIndex;
			newNode->m_leafEntries.ReserveExact(newNodeEntries);

			for (sizet i=splitIndex; i!=node.m_leafEntries.Len(); ++i)
				newNode->m_leafEntries.Add(std::move(node.m_leafEntries[i]));

			node.m_leafEntries.ResizeExact(splitIndex);

			newNodeFirstName = newNode->m_leafEntries.First().m_name;
		}
		else
		{
			EnsureThrow(!node.m_leafEntries.Any());
			EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());
			EnsureThrowWithNr2(splitIndex + 1 < node.m_branchEntries.Len(), splitIndex, node.m_branchEntries.Len());

			sizet const newNodeEntries = node.m_branchEntries.Len() - splitIndex;
			newNode->m_branchEntries.ReserveExact(newNodeEntries);
			newNode->m_childNodes   .ReserveExact(newNodeEntries);

			for (sizet i=splitIndex; i!=node.m_branchEntries.Len(); ++i)
			{
				newNode->m_branchEntries.Add(std::move(node.m_branchEntries[i]));
				newNode->m_childNodes.Add(node.m_childNodes[i]);
			}

			node.m_branchEntries.ResizeExact(splitIndex);
			node.m_childNodes.ResizeExact(splitIndex);

			newNodeFirstName = newNode->m_branchEntries.First().m_name;
		}

		ChangeState(node, NodeState::Changed);

		// Insert the new node into the hierarchy
		EnsureThrowWithNr(1 < navPath.Len(), navPath.Len());
		DirNavPath navPathNew { navPath };
		navPathNew.PopLast();
		++(navPathNew.Last().m_pos);

		DirBranchEntry entry;
		entry.m_blockIndex = newNode->m_block->BlockIndex();
		entry.m_name = newNodeFirstName;

		AddBranchEntryAt(entry, navPathNew, newNode, CanAddNode::Yes);
		
		// Update navPath
		sizet navPosOld = navPath.Last().m_pos;
		if (navPosOld < splitIndex)
			RebuildNavPath(navPath, node, navPosOld);
		else
		{
			DirNavEntry& navEntryNew = navPathNew.Add();
			navEntryNew.m_node = newNode;
			navEntryNew.m_pos = navPosOld - splitIndex;

			navPath = navPathNew;
		}
	}


	void Afs::DirCxRW::RebuildNavPath(DirNavPath& navPath, DirNode& node, sizet pos)
	{
		Seq name;
		if (0 == node.m_level)
		{
			EnsureThrowWithNr2(pos < node.m_leafEntries.Len(), pos, node.m_leafEntries.Len());
			name = node.m_leafEntries.First().m_name;
		}
		else
		{
			EnsureThrowWithNr2(pos < node.m_branchEntries.Len(), pos, node.m_branchEntries.Len());
			name = node.m_branchEntries.First().m_name;
		}

		navPath.Clear();
		navPath.Add().m_node = m_topNode;

		while (true)
		{
			DirNavEntry& curEntry = navPath.Last();
			if (curEntry.m_node == &node)
			{
				curEntry.m_pos = pos;
				break;
			}

			EnsureThrow(nullptr != curEntry.m_node);
			DirNode& curNode = *curEntry.m_node;
			EnsureThrow(0 != curNode.m_level);

			sizet foundPos {};
			FindResult fr = m_afs.FindNameEqualOrLessThan(curNode.m_branchEntries, name, foundPos);
			EnsureThrow(FindResult::NoEntries != fr);			// A branch node may not be empty
			EnsureThrow(FindResult::FirstIsGreater != fr);		// The name must be within this branch

			EnsureThrowWithNr2(curNode.m_branchEntries.Len() == curNode.m_childNodes.Len(), curNode.m_branchEntries.Len(), curNode.m_childNodes.Len());
			curEntry.m_pos = foundPos;
			navPath.Add().m_node = curNode.m_childNodes[foundPos];
		}
	}



	// Afs::FileNode

	void Afs::FileNode::Decode()
	{
		NodeView node = m_block->AsNodeView();
		m_isTop = (node.GetNodeCat() == NodeCat::Top);

		FileView file = node.AsAnyFileView();
		m_level = file.GetFileNodeLevel();

		if (NodeLevel_BeyondMax != m_level)
		{
			if (0 == m_level)
			{
				file.AsLeafView().DecodeEntries(m_leafEntries);
				m_dataBlocks.Clear().ResizeExact(m_leafEntries.Len());
			}
			else
			{
				file.AsBranchView().DecodeEntries(m_branchEntries);
				m_childNodes.Clear().ResizeExact(m_branchEntries.Len());
			}
		}
	}


	void Afs::FileNode::Encode()
	{
		NodeView node = m_block->AsNodeView();
		EnsureThrow((node.GetNodeCat() == NodeCat::Top) == m_isTop);

		FileView file = node.AsAnyFileView();
		EnsureThrowWithNr2(file.GetFileNodeLevel() == (byte) m_level, file.GetFileNodeLevel(), m_level);

		if (NodeLevel_BeyondMax == m_level)
		{
			EnsureThrowWithNr(0 == m_leafEntries.Len(), m_leafEntries.Len());
			EnsureThrowWithNr(0 == m_dataBlocks.Len(), m_dataBlocks.Len());
			EnsureThrowWithNr(0 == m_branchEntries.Len(), m_branchEntries.Len());
			EnsureThrowWithNr(0 == m_childNodes.Len(), m_childNodes.Len());
		}
		else
		{
			if (0 == m_level)
			{
				EnsureThrowWithNr(0 == m_branchEntries.Len(), m_branchEntries.Len());
				EnsureThrowWithNr(0 == m_childNodes.Len(), m_childNodes.Len());
				EnsureThrowWithNr2(m_leafEntries.Len() == m_dataBlocks.Len(), m_leafEntries.Len(), m_dataBlocks.Len());
				file.AsLeafView().EncodeEntries(m_leafEntries);
			}
			else
			{
				EnsureThrowWithNr(0 == m_leafEntries.Len(), m_leafEntries.Len());
				EnsureThrowWithNr(0 == m_dataBlocks.Len(), m_dataBlocks.Len());
				EnsureThrowWithNr2(m_branchEntries.Len() == m_childNodes.Len(), m_branchEntries.Len(), m_childNodes.Len());
				file.AsBranchView().EncodeEntries(m_branchEntries);
			}
		}
	}


	uint32 Afs::FileNode::AsIf_EncodedSizeOverhead(uint32 level)
	{
		FileView file = m_block->AsNodeView().AsAnyFileView();
		if (!level)
			return file.AsIf_LeafView().EncodedSizeOverhead();
		else
			return file.AsIf_BranchView().EncodedSizeOverhead();
	}


	uint32 Afs::FileNode::EncodedSizeEntries()
	{
		FileView file = m_block->AsNodeView().AsAnyFileView();
		if (!m_level)
			return file.AsLeafView().EncodedSizeEntries(m_leafEntries);
		else
			return file.AsBranchView().EncodedSizeEntries(m_branchEntries);
	}



	// Afs::FileCxR

	AfsResult::E Afs::FileCxR::GetFileTopBlock(ObjId id)
	{
		EnsureThrow(!m_topNode);
		m_nodes.ReserveInc(1);
		m_topNode = (m_nodes.Add() = new FileNode).Ptr();
		m_topNode->m_block = new VarBlock { m_jw };
		
		AfsResult::E r = m_afs.GetTopBlock(m_topNode->m_block.Ref(), id, ObjType::File);
		if (AfsResult::OK != r)
			return r;

		m_topNode->Decode();
		return AfsResult::OK;
	}


	void Afs::FileCxR::GetDataBlocks(uint64 offsetFirst, uint64 offsetBeyondLast, RpVec<VarBlock>& blocks)
	{
		EnsureThrow(nullptr != m_topNode);
		EnsureThrowWithNr2(offsetFirst <= offsetBeyondLast, offsetFirst, offsetBeyondLast);
		EnsureThrowWithNr2(offsetBeyondLast - offsetFirst < SIZE_MAX, offsetFirst, offsetBeyondLast);

		sizet totalBytes = (sizet) (offsetBeyondLast - offsetFirst);
		blocks.Clear();
		blocks.ReserveExact((totalBytes / m_afs.m_blockSize) + 2);

		FileNavPath navPath;
		NavToLeafEntryContainingOffset(navPath, offsetFirst);

		FileNavPath navPathEnd;
		if (offsetFirst == offsetBeyondLast)
			navPathEnd = navPath;
		else
			NavToLeafEntryContainingOffset(navPathEnd, offsetBeyondLast - 1);

		EnsureThrow(navPathEnd.Any());
		FileNavEntry const& navEntryEnd = navPathEnd.Last();
		EnsureThrow(nullptr != navEntryEnd.m_node);
		FileNode const& nodeEnd = *navEntryEnd.m_node;

		while (true)
		{
			FileNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			FileNode& node = *navEntry.m_node;
			EnsureThrowWithNr(0 == node.m_level, node.m_level);

			Rp<VarBlock>& block = GetDataBlock(node, navEntry.m_pos);
			blocks.Add() = block;

			if (&nodeEnd == &node)
				if (navEntryEnd.m_pos == navEntry.m_pos)
					break;

			++(navEntry.m_pos);
			if (node.m_leafEntries.Len() == navEntry.m_pos)
				NavToSiblingNode(navPath, EnumDir::Forward);
		}
	}


	Rp<Afs::VarBlock>& Afs::FileCxR::GetDataBlock(FileNode& node, sizet i)
	{
		Rp<VarBlock>& block = node.m_dataBlocks[i];
		if (!block.Any())
		{
			FileLeafEntry const& leafEntry = node.m_leafEntries[i];
			block = new VarBlock { m_jw };
			AfsResult::E r = m_afs.m_storage->ObtainBlock(block.Ref(), leafEntry.m_blockIndex);
			EnsureThrowWithNr2(AfsResult::OK == r, r, leafEntry.m_blockIndex);
		}
		return block;
	}


	void Afs::FileCxR::NavToLeafEntryContainingOffset(FileNavPath& navPath, uint64 offset)
	{
		navPath.Clear();
		navPath.Add().m_node = m_topNode;

		while (true)
		{
			FileNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			FileNode& node = *navEntry.m_node;
			EnsureThrowWithNr(NodeLevel_BeyondMax > node.m_level, node.m_level);

			if (!node.m_level)
			{
				FileLeafView leaf = node.m_block->AsNodeView().AsAnyFileView().AsLeafView();
				EnsureThrowWithNr2(0 == (leaf.GetFileOffset() % m_afs.m_blockSize), leaf.GetFileOffset(), m_afs.m_blockSize);
				EnsureThrowWithNr2(offset >= leaf.GetFileOffset(), offset, leaf.GetFileOffset());
				EnsureThrowWithNr2(offset - leaf.GetFileOffset() < SIZE_MAX, offset, leaf.GetFileOffset());

				sizet const relativeOffset = (sizet) (offset - leaf.GetFileOffset());
				navEntry.m_pos = (relativeOffset / m_afs.m_blockSize);
				EnsureThrowWithNr2(navEntry.m_pos < node.m_leafEntries.Len(), navEntry.m_pos, node.m_leafEntries.Len());
				break;
			}
			else
			{
				EnsureThrow(node.m_branchEntries.Any());
				EnsureThrowWithNr2(offset >= node.m_branchEntries.First().m_fileOffset, offset, node.m_branchEntries.First().m_fileOffset);

				sizet i = node.m_branchEntries.Len();
				do
				{
					--i;
					if (offset >= node.m_branchEntries[i].m_fileOffset)
						break;
				}
				while (i);

				navEntry.m_pos = i;
				DescendToNextChildNode(navPath, EnumDir::Forward);
			}
		}
	}


	void Afs::FileCxR::DescendToNextChildNode(FileNavPath& navPath, EnumDir enumDir)
	{
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrow(0 != node.m_level);
		EnsureThrowWithNr2(node.m_childNodes.Len() > navEntry.m_pos, node.m_childNodes.Len(), navEntry.m_pos);

		FileNode*& pChildNode = node.m_childNodes[navEntry.m_pos];
		if (!pChildNode)
		{
			m_nodes.ReserveInc(1);
			pChildNode = (m_nodes.Add() = new FileNode).Ptr();

			FileNode& childNode = *pChildNode;
			EnsureThrowWithNr2(node.m_branchEntries.Len() > navEntry.m_pos, node.m_branchEntries.Len(), navEntry.m_pos);
			FileBranchEntry& branchEntry = node.m_branchEntries[navEntry.m_pos];
			childNode.m_block = new VarBlock { m_jw };
			AfsResult::E r = childNode.m_block->ObtainBlock_CheckKind(m_afs.m_storage, branchEntry.m_blockIndex, BlockKind::Node);
			EnsureThrowWithNr(AfsResult::OK == r, r);
			childNode.Decode();
			EnsureThrowWithNr2(childNode.m_level + 1 == node.m_level, childNode.m_level, node.m_level);
		}
				
		navPath.Add().m_node = pChildNode;

		if (EnumDir::Reverse == enumDir)
		{
			sizet const nrEntries = pChildNode->NrVecEntries();
			EnsureThrow(0 != nrEntries);
			navPath.Last().m_pos = nrEntries - 1;
		}
	}


	bool Afs::FileCxR::NavToSiblingNode(FileNavPath& navPath, EnumDir enumDir)
	{
		// Get current node level
		uint32 targetLevel;

		{
			FileNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			targetLevel = navEntry.m_node->m_level;
		}

		// Ascend until we find a higher level node which allows us to move in the desired direction
		uint32 childLevel = targetLevel;
		while (true)
		{
			uint64 childBlockIndex = navPath.Last().m_node->m_block->BlockIndex();
			navPath.PopLast();

			// We have exhausted higher levels and still cannot move in the desired direction. Return with "navPath" cleared
			if (!navPath.Any())
				return false;

			// Verify that the current parent position corresponds to child from which we ascended
			FileNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			FileNode& node = *navEntry.m_node;
			EnsureThrowWithNr2(childLevel + 1 == node.m_level, childLevel, node.m_level);
			EnsureThrowWithNr2(node.m_branchEntries.Len() > navEntry.m_pos, node.m_branchEntries.Len(), navEntry.m_pos);
			FileBranchEntry& branchEntry = node.m_branchEntries[navEntry.m_pos];
			EnsureThrowWithNr2(branchEntry.m_blockIndex == childBlockIndex, branchEntry.m_blockIndex, childBlockIndex);

			// Can we move in the desired lateral direction?
			if (EnumDir::Forward == enumDir)
			{
				if (navEntry.m_pos + 1 < node.m_branchEntries.Len())
				{
					++(navEntry.m_pos);
					break;
				}
			}
			else if (EnumDir::Reverse == enumDir)
			{
				if (navEntry.m_pos > 0)
				{
					--(navEntry.m_pos);
					break;
				}
			}
			else
				EnsureThrow(!"Unrecognized EnumDir");

			// We can't yet move in the desired lateral direction. Keep going up
			++childLevel;
		}

		// Descend to sibling at target level
		while (true)
		{
			DescendToNextChildNode(navPath, enumDir);

			FileNavEntry& navEntry = navPath.Last();
			FileNode& node = *navEntry.m_node;
			if (node.m_level == targetLevel)
				break;

			EnsureThrow(0 != node.m_level);
			EnsureThrow(node.m_branchEntries.Any());
		}

		return true;
	}



	// Afs::FileCxRW

	uint64 Afs::FileCxRW::ImpliedCapacity(uint64 sizeBytes) const
	{
		uint64 cap = sizeBytes;
		uint32 capOverBlockSize = (uint32) (cap % m_afs.m_blockSize);
		if (0 != capOverBlockSize)
		{
			cap += (m_afs.m_blockSize - capOverBlockSize);
			capOverBlockSize = (uint32) (cap % m_afs.m_blockSize);
			EnsureThrowWithNr(0 == capOverBlockSize, capOverBlockSize);
		}
		return cap;
	}


	void Afs::FileCxRW::EnlargeToSize(uint64 newSizeBytes)
	{
		EnsureThrow(nullptr != m_topNode);
		
		TopView top = m_topNode->m_block->AsNodeView().AsTopView();
		uint64 const prevSizeBytes = top.Get_File_SizeBytes();
		EnsureThrowWithNr2(prevSizeBytes < newSizeBytes, prevSizeBytes, newSizeBytes);

		FileView fileView = top.AsFileView();
		if (newSizeBytes <= fileView.AsIf_MiniView().ViewLen())
		{
			// Intended size fits in a mini node. In this case, existing node must already be a mini node.
			EnsureThrow(fileView.IsMini());
		}
		else
		{
			// Intended size does NOT fit in a mini node
			if (fileView.IsMini())
			{
				// Convert from existing mini-node into leaf node + data block
				FileMiniView mini = fileView.AsMiniView();
				EnsureThrowWithNr2(prevSizeBytes <= mini.ViewLen(), prevSizeBytes, mini.ViewLen());
				uint32 prevSize32 = (uint32) prevSizeBytes;

				Rp<VarBlock> dataBlock = m_jw->ReclaimBlockOrAddNew(BlockKind::None);
				EnsureThrowWithNr2(prevSize32 <= m_afs.m_blockSize, prevSize32, m_afs.m_blockSize);
				Mem::Copy<byte>(dataBlock->WritePtr(), mini.ReadPtr(), prevSize32);

				fileView.SetFileNodeLevel() = 0;

				FileLeafView leafView = fileView.AsLeafView();
				leafView.SetFileOffset() = 0;

				m_topNode->m_level = 0;
				EnsureThrowWithNr(0 == m_topNode->m_leafEntries.Len(), m_topNode->m_leafEntries.Len());
				m_topNode->m_leafEntries.Add().m_blockIndex = dataBlock->BlockIndex();
				m_topNode->m_dataBlocks.Add() = dataBlock;
				ChangeState(*m_topNode, NodeState::Changed);
			}

			// Can intended size fit into current number of blocks?
			uint64 curCap = ImpliedCapacity(prevSizeBytes);
			if (0 == curCap)
				curCap = m_afs.m_blockSize;

			if (newSizeBytes > curCap)
			{
				// We need more blocks. Navigate to last leaf node
				uint64 navToOffset = prevSizeBytes;
				if (navToOffset)
					--navToOffset;

				FileNavPath navPath;
				NavToLeafEntryContainingOffset(navPath, navToOffset);

				FileNavEntry& navEntry = navPath.Last();
				EnsureThrow(nullptr != navEntry.m_node);
				FileNode& node = *navEntry.m_node;
				EnsureThrowWithNr(0 == node.m_level, node.m_level);
				EnsureThrowWithNr2(navEntry.m_pos + 1 == node.m_leafEntries.Len(), navEntry.m_pos, node.m_leafEntries.Len());

				// Add blocks until we have enough
				do
				{
					Rp<VarBlock> dataBlock = m_jw->ReclaimBlockOrAddNew(BlockKind::None);
					AddLeafEntryAtEnd(dataBlock, navPath, CanAddNode::Yes);
					curCap += m_afs.m_blockSize;
				}
				while (newSizeBytes > curCap);
			}
		}

		top.Set_File_SizeBytes() = newSizeBytes;
	}


	void Afs::FileCxRW::ShrinkToSize(uint64 newSizeBytes)
	{
		EnsureThrow(nullptr != m_topNode);

		TopView top = m_topNode->m_block->AsNodeView().AsTopView();
		uint64 const prevSizeBytes = top.Get_File_SizeBytes();
		EnsureThrowWithNr2(newSizeBytes < prevSizeBytes, newSizeBytes, prevSizeBytes);

		FileView fileView = top.AsFileView();
		FileMiniView mini = fileView.AsIf_MiniView();
		if (prevSizeBytes <= mini.ViewLen())
		{
			// Existing size fits in a mini node. In this case, the node must be a mini node, and remains one
			EnsureThrow(fileView.IsMini());
			sizet nrToZero = (sizet) (prevSizeBytes - newSizeBytes);
			Mem::Zero<byte>(mini.WritePtr() + newSizeBytes, nrToZero);
		}
		else
		{
			EnsureThrow(!fileView.IsMini());

			uint64 curCap = ImpliedCapacity(prevSizeBytes);
			uint64 newCap = ImpliedCapacity(newSizeBytes);
			if (newCap < curCap)
			{
				// We need to remove blocks. Navigate to last leaf node
				uint64 navToOffset = prevSizeBytes;
				if (navToOffset)
					--navToOffset;

				FileNavPath navPath;
				NavToLeafEntryContainingOffset(navPath, navToOffset);

				// Remove blocks until we're at new cap
				do
				{
					RemoveDataBucketAtEnd(navPath);
					curCap -= m_afs.m_blockSize;
				}
				while (curCap > newCap);
			}

			if (0 == newSizeBytes)
			{
				// No blocks left. Convert from empty leaf node into an empty mini-node
				EnsureThrowWithNr(0 == m_topNode->m_level, m_topNode->m_level);
				EnsureThrowWithNr(0 == m_topNode->m_leafEntries.Len(), m_topNode->m_leafEntries.Len());
				EnsureThrowWithNr(0 == m_topNode->m_dataBlocks.Len(), m_topNode->m_dataBlocks.Len());

				fileView.SetFileNodeLevel() = NodeLevel_BeyondMax;
				Mem::Zero<byte>(mini.WritePtr(), mini.ViewLen());

				m_topNode->m_level = NodeLevel_BeyondMax;
				ChangeState(*m_topNode, NodeState::Changed);
			}
			else if (newSizeBytes <= mini.ViewLen())
			{
				// New size fits into a mini node. Convert from existing leaf node + data block into a mini-node
				EnsureThrowWithNr(0 == m_topNode->m_level, m_topNode->m_level);
				EnsureThrowWithNr(1 == m_topNode->m_leafEntries.Len(), m_topNode->m_leafEntries.Len());
				EnsureThrowWithNr(1 == m_topNode->m_dataBlocks.Len(), m_topNode->m_dataBlocks.Len());

				uint32 newSize32 = (uint32) newSizeBytes;
				Rp<VarBlock>& block = GetDataBlock(*m_topNode, 0);
				EnsureThrowWithNr2(newSize32 <= m_afs.m_blockSize, newSize32, m_afs.m_blockSize);

				fileView.SetFileNodeLevel() = NodeLevel_BeyondMax;
				Mem::Copy<byte>(mini.WritePtr(), block->ReadPtr(), newSize32);
				Mem::Zero<byte>(mini.WritePtr() + newSize32, mini.ViewLen() - newSize32);

				m_jw->AddBlockToFree(block);

				m_topNode->m_level = NodeLevel_BeyondMax;
				m_topNode->m_leafEntries.Clear();
				m_topNode->m_dataBlocks.Clear();
				ChangeState(*m_topNode, NodeState::Changed);
			}
			else
			{
				// New size does NOT fit into a mini node. Zero out any extra data in the new last block
				sizet bytesInLastBlock = (sizet) (newSizeBytes % m_afs.m_blockSize);
				if (0 != bytesInLastBlock)
				{
					FileNavPath navPath;
					NavToLeafEntryContainingOffset(navPath, newSizeBytes - 1);

					FileNavEntry& navEntry = navPath.Last();
					EnsureThrow(nullptr != navEntry.m_node);
					FileNode& node = *navEntry.m_node;
					EnsureThrowWithNr(0 == node.m_level, node.m_level);

					Rp<VarBlock>& block = GetDataBlock(node, navEntry.m_pos);
					Mem::Zero<byte>(block->WritePtr() + bytesInLastBlock, m_afs.m_blockSize - bytesInLastBlock);
				}
			}
		}

		top.Set_File_SizeBytes() = newSizeBytes;
	}


	void Afs::FileCxRW::Finalize()
	{
		if (!m_anyChanged)
			return;

		m_jw->DecrementFinalizationsPending();

		for (Rp<FileNode> const& node : m_nodes)
		{
			if (node->m_state == NodeState::Changed)
			{
				node->Encode();
				node->m_state = NodeState::Finalized;
			}
			else if (node->m_state == NodeState::Free)
			{
				m_jw->AddBlockToFree(node->m_block);
				node->m_state = NodeState::Finalized;
			}
		}
	}


	void Afs::FileCxRW::AddLeafEntryAtEnd(Rp<VarBlock> const& dataBlock, FileNavPath& navPath, CanAddNode canAddNode)
	{
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrowWithNr(0 == node.m_level, node.m_level);

		if (node.EncodedSize() + FileLeafEntry::EncodedSize <= m_afs.m_blockSize)
		{
			// The entry can be appended in this node
			EnsureThrowWithNr2(node.m_leafEntries.Len() == node.m_dataBlocks.Len(), node.m_leafEntries.Len(), node.m_dataBlocks.Len());
			node.m_leafEntries.Add().m_blockIndex = dataBlock->BlockIndex();
			node.m_dataBlocks.Add(dataBlock);
			ChangeState(node, NodeState::Changed);

			navEntry.m_pos = node.m_leafEntries.Len() - 1;
		}
		else
		{
			EnsureThrow(CanAddNode::Yes == canAddNode);
			uint64 const curNodeOffset = node.m_block->AsNodeView().AsAnyFileView().AsLeafView().GetFileOffset();
			uint64 const newBlockOffset = curNodeOffset + (m_afs.m_blockSize * node.m_leafEntries.Len());
			MakeRoomForEntryAtEnd(navPath, newBlockOffset, FileLeafEntry::EncodedSize);
			AddLeafEntryAtEnd(dataBlock, navPath, CanAddNode::No);
		}
	}


	void Afs::FileCxRW::MakeRoomForEntryAtEnd(FileNavPath& navPath, uint64 newBlockOffset, sizet entrySize)
	{
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;

		if (!node.m_isTop)
			AddNonTopNodeAtEnd(navPath, newBlockOffset);
		else
		{
			// Node is top. Separate it into top + child
			sizet bytesGained = SplitTopNode(navPath, node);
			EnsureThrow(2 == navPath.Len());

			// Separation may have created enough room. If not, recurse
			if (bytesGained < entrySize)
				AddNonTopNodeAtEnd(navPath, newBlockOffset);
		}
	}


	sizet Afs::FileCxRW::SplitTopNode(FileNavPath& navPath, FileNode& node)
	{
		EnsureThrowWithNr(1 == navPath.Len(), navPath.Len());
		EnsureThrow(node.m_isTop);

		sizet const prevEncodedSize = node.EncodedSize();

		m_nodes.ReserveInc(1);
			
		FileNode* child = (m_nodes.Add() = new FileNode).Ptr();
		child->m_block = m_jw->ReclaimBlockOrAddNew(BlockKind::Node);
		child->m_level = node.m_level;
		ChangeState(*child, NodeState::Changed);

		NodeView nodeView = child->m_block->AsNodeView();
		nodeView.SetNodeCat() = NodeCat::NonTop;
		nodeView.SetObjType() = ObjType::File;

		FileView fileView = nodeView.AsNonTopFileView();
		fileView.SetFileNodeLevel() = (byte) node.m_level;

		if (0 == node.m_level)
		{
			EnsureThrow(!node.m_branchEntries.Any());
			EnsureThrow(!node.m_childNodes.Any());
			EnsureThrow(0 != node.m_leafEntries.Len());
			EnsureThrowWithNr2(node.m_leafEntries.Len() == node.m_dataBlocks.Len(), node.m_leafEntries.Len(), node.m_dataBlocks.Len());

			fileView.AsLeafView().SetFileOffset() = 0;

			child->m_leafEntries = std::move(node.m_leafEntries );
			child->m_dataBlocks  = std::move(node.m_dataBlocks  );
		}
		else
		{
			EnsureThrow(!node.m_leafEntries.Any());
			EnsureThrow(!node.m_dataBlocks.Any());
			EnsureThrow(0 != node.m_branchEntries.Len());
			EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());

			child->m_branchEntries = std::move(node.m_branchEntries );
			child->m_childNodes    = std::move(node.m_childNodes    );
		}

		// Update top node
		++(node.m_level);
		EnsureThrow(NodeLevel_BeyondMax > node.m_level);
		node.m_block->AsNodeView().AsTopView().AsFileView().SetFileNodeLevel() = (byte) node.m_level;

		node.m_leafEntries.Clear();

		node.m_branchEntries.ResizeExact(1);
		node.m_branchEntries[0].m_blockIndex = child->m_block->BlockIndex();
		node.m_branchEntries[0].m_fileOffset = 0;
		
		node.m_childNodes.ResizeExact(1);
		node.m_childNodes[0] = child;
		
		ChangeState(node, NodeState::Changed);

		// Update navPath
		FileNavEntry& navEntryOld = navPath.Last();
		sizet navPosOld = navEntryOld.m_pos;
		navEntryOld.m_pos = 0;

		FileNavEntry& navEntryNew = navPath.Add();
		navEntryNew.m_node = child;
		navEntryNew.m_pos = navPosOld;

		// Calculate bytes gained
		sizet bytesGained {};
		sizet const childEncodedSize = child->EncodedSize();
		if (prevEncodedSize > childEncodedSize)
			bytesGained = prevEncodedSize - childEncodedSize;
		return bytesGained;
	}


	void Afs::FileCxRW::AddNonTopNodeAtEnd(FileNavPath& navPath, uint64 newBlockOffset)
	{
		EnsureThrowWithNr2((newBlockOffset % m_afs.m_blockSize) == 0, newBlockOffset, m_afs.m_blockSize);
		EnsureThrow(1 < navPath.Len());
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrow(!node.m_isTop);

		FileNode* newNode = (m_nodes.Add() = new FileNode).Ptr();
		newNode->m_block = m_jw->ReclaimBlockOrAddNew(BlockKind::Node);
		newNode->m_level = node.m_level;
		ChangeState(*newNode, NodeState::Changed);

		NodeView newNodeView = newNode->m_block->AsNodeView();
		newNodeView.SetNodeCat() = NodeCat::NonTop;
		newNodeView.SetObjType() = ObjType::File;
		newNodeView.AsNonTopFileView().SetFileNodeLevel() = (byte) node.m_level;

		if (0 == node.m_level)
		{
			FileLeafView leaf = newNodeView.AsNonTopFileView().AsLeafView();
			leaf.SetFileOffset() = newBlockOffset;
		}

		// Insert the new node into the hierarchy
		FileNavPath navPathNew { navPath };
		navPathNew.PopLast();

		FileBranchEntry entry;
		entry.m_blockIndex = newNode->m_block->BlockIndex();
		entry.m_fileOffset = newBlockOffset;

		AddBranchEntryAtEnd(entry, navPathNew, newNode, CanAddNode::Yes);
		
		// Update navPath
		FileNavEntry& navEntryNew = navPathNew.Add();
		navEntryNew.m_node = newNode;
		navEntryNew.m_pos = 0;

		navPath = navPathNew;
	}


	void Afs::FileCxRW::AddBranchEntryAtEnd(FileBranchEntry& entry, FileNavPath& navPath, FileNode* newNode, CanAddNode canAddNode)
	{
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrow(0 != node.m_level);

		if (node.EncodedSize() + FileBranchEntry::EncodedSize <= m_afs.m_blockSize)
		{
			// The entry can be inserted into this node
			EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());
			node.m_branchEntries.Add(entry);
			node.m_childNodes.Add(newNode);

			sizet const nrEntries = node.m_branchEntries.Len();
			if (1 < nrEntries)
			{
				uint64 offset1 = node.m_branchEntries[nrEntries-2].m_fileOffset;
				uint64 offset2 = node.m_branchEntries[nrEntries-1].m_fileOffset;
				EnsureThrowWithNr2(offset1 < offset2, offset1, offset2);
			}

			ChangeState(node, NodeState::Changed);
		}
		else
		{
			// Insufficient room to add entry. Must add new node block
			EnsureThrow(CanAddNode::Yes == canAddNode);
			MakeRoomForEntryAtEnd(navPath, entry.m_fileOffset, FileBranchEntry::EncodedSize);
			AddBranchEntryAtEnd(entry, navPath, newNode, CanAddNode::No);
		}
	}


	void Afs::FileCxRW::RemoveDataBucketAtEnd(FileNavPath& navPath)
	{
		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrowWithNr(0 == node.m_level, node.m_level);
		EnsureThrowWithNr2(navEntry.m_pos + 1 == node.m_leafEntries.Len(), navEntry.m_pos, node.m_leafEntries.Len());
		EnsureThrowWithNr2(node.m_leafEntries.Len() == node.m_dataBlocks.Len(), node.m_leafEntries.Len(), node.m_dataBlocks.Len());

		Rp<VarBlock>& block = GetDataBlock(node, navEntry.m_pos);
		m_jw->AddBlockToFree(block);

		node.m_leafEntries.PopLast();
		node.m_dataBlocks.PopLast();
		if (0 != navEntry.m_pos)
			--(navEntry.m_pos);
		ChangeState(node, NodeState::Changed);

		if (2 == navPath.Len() && 1 == m_topNode->m_branchEntries.Len())
			if (TryHoistIntoTopNode(navPath))
				return;

		if (node.m_leafEntries.Any())
		{
			// Leaf node still contains at least one entry
		}
		else if (node.m_isTop)
		{
			// Leaf node is empty, but it's the top node. Nothing to do here
			// The top node might need to become a mini-node, but such a decision is made by the caller
		}
		else
		{
			// Leaf node is empty and non-top. Remove the node and its parent branch entry
			RemoveNodeAtEnd_NavToPrevSibling(navPath);
		}
	}


	void Afs::FileCxRW::RemoveNodeAtEnd_NavToPrevSibling(FileNavPath& navPath)
	{
		{
			EnsureThrowWithNr(1 < navPath.Len(), navPath.Len());
			FileNavEntry& navEntry = navPath.Last();
			EnsureThrow(nullptr != navEntry.m_node);
			FileNode& node = *navEntry.m_node;
			EnsureThrowWithNr(0 == node.m_leafEntries.Len(), node.m_leafEntries.Len());
			EnsureThrowWithNr(0 == node.m_branchEntries.Len(), node.m_branchEntries.Len());

			ChangeState(node, NodeState::Free);
			navPath.PopLast();
		}

		FileNavEntry& navEntry = navPath.Last();
		EnsureThrow(nullptr != navEntry.m_node);
		FileNode& node = *navEntry.m_node;
		EnsureThrowWithNr2(navEntry.m_pos + 1 == node.m_branchEntries.Len(), navEntry.m_pos, node.m_branchEntries.Len());
		EnsureThrowWithNr2(node.m_branchEntries.Len() == node.m_childNodes.Len(), node.m_branchEntries.Len(), node.m_childNodes.Len());

		node.m_branchEntries.PopLast();
		node.m_childNodes.PopLast();
		if (0 != navEntry.m_pos)
			--(navEntry.m_pos);
		ChangeState(node, NodeState::Changed);

		if (2 == navPath.Len() && 1 == m_topNode->m_branchEntries.Len())
			if (TryHoistIntoTopNode(navPath))
			{
				DescendToNextChildNode(navPath, EnumDir::Reverse);
				return;
			}

		if (node.m_branchEntries.Any())
		{
			// Branch node still contains at least one entry
			DescendToNextChildNode(navPath, EnumDir::Reverse);

			if (2 == navPath.Len() && 1 == m_topNode->m_branchEntries.Len())
				TryHoistIntoTopNode(navPath);
		}
		else
		{
			// Branch node is empty and non-top. Remove the node and its parent branch entry
			EnsureThrow(!node.m_isTop);
			RemoveNodeAtEnd_NavToPrevSibling(navPath);
			DescendToNextChildNode(navPath, EnumDir::Reverse);
		}
	}


	bool Afs::FileCxRW::TryHoistIntoTopNode(FileNavPath& navPath)
	{
		// When the top node has exactly one entry remaining, the child node (leaf or branch) needs to be hoisted into the top node.
		// However, since a top node contains extra information which a non-top node does not contain, the top node might not have
		// enough space to contain the entire child node when the number of top node entries is first reduced to 1.
		//
		// For this reason, we check to hoist the child node into the top node on two occasions:
		// - When the number of top node entries is first reduced to 1.
		// - When an immediate child node entry is removed, and the number of top node entries is already 1.

		EnsureThrowWithNr(2 == navPath.Len(), navPath.Len());
		FileNavEntry& topNavEntry = navPath.First();
		EnsureThrow(nullptr != topNavEntry.m_node);
		FileNode& topNode = *topNavEntry.m_node;
		EnsureThrow(0 != topNode.m_level);
		EnsureThrow(NodeLevel_BeyondMax > topNode.m_level);
		EnsureThrowWithNr(1 == topNode.m_branchEntries.Len(), topNode.m_branchEntries.Len());
		EnsureThrowWithNr(0 == topNavEntry.m_pos, topNavEntry.m_pos);

		FileNavEntry& childNavEntry = navPath.Last();
		EnsureThrow(nullptr != childNavEntry.m_node);
		FileNode& childNode = *childNavEntry.m_node;

		if (topNode.AsIf_EncodedSizeOverhead(childNode.m_level) + childNode.EncodedSizeEntries() > m_afs.m_blockSize)
			return false;

		// Child node does fit into top node. Hoist it
		FileView fileView = topNode.m_block->AsNodeView().AsTopView().AsFileView();
		fileView.SetFileNodeLevel() = (byte) childNode.m_level;
		topNode.m_level = childNode.m_level;

		if (0 == childNode.m_level)
		{
			FileLeafView leaf = fileView.AsLeafView();
			leaf.SetFileOffset() = childNode.m_block->AsNodeView().AsNonTopFileView().AsLeafView().GetFileOffset();
			topNode.m_leafEntries = std::move(childNode.m_leafEntries);
			topNode.m_dataBlocks = std::move(childNode.m_dataBlocks);
			topNode.m_branchEntries.Clear();
			topNode.m_childNodes.Clear();
		}
		else
		{
			topNode.m_branchEntries = std::move(childNode.m_branchEntries);
			topNode.m_childNodes = std::move(childNode.m_childNodes);
		}

		ChangeState(topNode, NodeState::Changed);
		ChangeState(childNode, NodeState::Free);

		topNavEntry.m_pos = childNavEntry.m_pos;
		navPath.PopLast();
		return true;
	}



	// Afs: public interface

	void Afs::SetStorage(AfsStorage& storage)
	{
		EnsureThrow(m_state == State::Uninited);
		m_storage = &storage;
	}

	
	void Afs::SetNameComparer(NameComparer cmp)
	{
		EnsureThrow(m_state == State::Uninited);
		m_cmp = cmp;
	}

	
	AfsResult::E Afs::Init(Seq rootDirMetaData, Time now)
	{
		EnsureThrow(m_state == State::Uninited);
		EnsureThrow(m_storage);
		StateGuard stateGuard { *this };

		bool const firstInit = (0 == m_storage->NrBlocks());
		AfsResult::E r;

		m_blockSize = m_storage->BlockSize();
		sizet const allocatorBlockSize = m_storage->Allocator().BytesPerBlock();
		EnsureThrowWithNr2(m_blockSize <= allocatorBlockSize, m_blockSize, allocatorBlockSize);

		m_masterBlock = new MasterBlock { nullptr };
		m_rootDirTopNode = new VarBlock { nullptr };
		m_freeListTailBlock = new VarBlock { nullptr };

		if (firstInit)
		{
			JournaledWrite jw { *this };

			r = m_storage->AddNewBlock(m_rootDirTopNode.Ref());
			EnsureThrowWithNr(AfsResult::OK == r, r);
			EnsureThrowWithNr(0 == ObjId::Root.m_index, ObjId::Root.m_index);
			EnsureThrowWithNr(0 == m_rootDirTopNode->BlockIndex(), m_rootDirTopNode->BlockIndex());
			m_rootDirTopNode->SetBlockKind() = BlockKind::Node;

			NodeView rootNodeView = m_rootDirTopNode->AsNodeView();
			rootNodeView.SetNodeCat() = NodeCat::Top;
			rootNodeView.SetObjType() = ObjType::Dir;

			TopView top = rootNodeView.AsTopView();
			top.SetUniqueId() = ObjId::Root.m_uniqueId;
			top.SetParentId(ObjId::None);
			top.Set_Dir_NrEntries() = 0;
			top.SetCreateTime() = now.ToFt();
			top.SetModifyTime() = top.GetCreateTime();
			top.SetMetaData(rootDirMetaData);

			DirView dir = top.AsDirView();
			dir.SetDirNodeLevel() = 0;

			DirLeafView leaf = dir.AsLeafView();
			leaf.SetNrEntries() = 0;

			r = m_storage->AddNewBlock(m_masterBlock.Ref());
			EnsureThrowWithNr(AfsResult::OK == r, r);
			EnsureThrowWithNr(m_masterBlock->BlockIndex() == 1, m_masterBlock->BlockIndex());
			m_masterBlock->SetBlockKind() = BlockKind::Master;
			m_masterBlock->SetFsVersion() = OurFsVersion;
			m_masterBlock->SetNextUniqueId() = ObjId::Root.m_uniqueId + 1;

			r = m_storage->AddNewBlock(m_freeListTailBlock.Ref());
			EnsureThrowWithNr(AfsResult::OK == r, r);
			EnsureThrowWithNr(m_freeListTailBlock->BlockIndex() == 2, m_masterBlock->BlockIndex());
			m_freeListTailBlock->SetBlockKind() = BlockKind::FreeList;

			FreeListView freeListView = m_freeListTailBlock->AsFreeListView();
			freeListView.SetPrevFreeListBlockIndex() = UINT64_MAX;
			freeListView.SetNrIndices() = 0;

			m_masterBlock->SetFreeListTailBlockIndex() = m_freeListTailBlock->BlockIndex();
			m_masterBlock->SetNrFullFreeListNodes() = 0;
			m_masterBlock->SetRootDirTopNodeIndex() = m_rootDirTopNode->BlockIndex();

			jw.Complete();
		}
		else
		{
			r = m_masterBlock->ObtainBlock_CheckKind(m_storage, 1, BlockKind::Master);
			EnsureThrowWithNr(AfsResult::OK == r, r);

			if (m_masterBlock->GetFsVersion() != OurFsVersion)
				return AfsResult::UnsupportedFsVersion;

			r = m_freeListTailBlock->ObtainBlock_CheckKind(m_storage, m_masterBlock->GetFreeListTailBlockIndex(), BlockKind::FreeList);
			EnsureThrowWithNr(AfsResult::OK == r, r);

			r = m_rootDirTopNode->ObtainBlock_CheckKind(m_storage, m_masterBlock->GetRootDirTopNodeIndex(), BlockKind::Node);
			EnsureThrowWithNr(AfsResult::OK == r, r);
		}

		// Calculate maximum name length:
		// - A DirLeafEntry is always larger than the corresponding DirBranchEntry, so we calculate on the basis of DirLeafEntry.
		// - A top node has more overhead (less space for entries) than a non-top node, so we calculate on the basis of the root top node.
		// - A name must be short enough so that a node which contains it can still meet its rebalance threshold.
		uint32 const topDirLeafNode_overhead = m_rootDirTopNode->AsNodeView().AsTopView().AsDirView().AsIf_LeafView().EncodedSizeOverhead();
		uint32 const topDirLeafNode_spaceForEntries = SatSub<uint32>(m_blockSize, topDirLeafNode_overhead);
		m_maxNameBytes = PickMin<uint32>(0xFFFFU, SatSub<uint32>(topDirLeafNode_spaceForEntries / NodeRebalanceThresholdFraction, DirLeafEntry::EncodedSizeOverhead));
		EnsureThrow(0 != m_maxNameBytes);
		m_maxMetaBytes = m_maxNameBytes;
		EnsureThrowWithNr2(rootDirMetaData.n <= m_maxMetaBytes, m_maxMetaBytes, rootDirMetaData.n);

		m_state = State::Inited;
		return stateGuard.Dismiss(AfsResult::OK);
	}


	uint64 Afs::FreeSpaceBlocks()
	{
		EnsureThrow(State::Inited == m_state);

		uint64 const maxNrBlocks = m_storage->MaxNrBlocks();
		if (UINT64_MAX == maxNrBlocks)
			return UINT64_MAX;

		uint64 const nrBlocks = m_storage->NrBlocks();
		uint64 const nrUnallocatedBlocks = SatSub(maxNrBlocks, nrBlocks);

		uint64 const nrFullFreeListNodes = m_masterBlock->GetNrFullFreeListNodes();

		FreeListView freeList = m_freeListTailBlock->AsFreeListView();
		uint64 const freeListBlock_maxNrIndices = freeList.MaxNrIndices(m_blockSize);
		uint64 const freeListTail_nrIndices = freeList.GetNrIndices();
		uint64 const nrFreeListBlocks = (nrFullFreeListNodes * freeListBlock_maxNrIndices) + nrFullFreeListNodes + freeListTail_nrIndices;

		uint64 const nrFreeSpaceBlocks = SatAdd(nrUnallocatedBlocks, nrFreeListBlocks);
		return nrFreeSpaceBlocks;
	}


	void Afs::VerifyFreeList()
	{
		EnsureThrow(State::Inited == m_state);

		OrderedSet<uint64> freeListBlockIndices;
		auto addIndex = [&] (uint64 blockIndex)
			{
				bool added {};
				OrderedSet<uint64>::It it = freeListBlockIndices.FindOrAdd(added, blockIndex);
				EnsureThrow(it.Any());
				EnsureThrow(added);
			};

		Rp<VarBlock> cur = m_freeListTailBlock;
		sizet nrFullFreeListNodes {};
	
		while (true)
		{
			EnsureThrowWithNr(cur->BlockIndex() > 1, cur->BlockIndex());
			addIndex(cur->BlockIndex());

			FreeListView freeList = cur->AsFreeListView();
			uint32 const nrIndices = freeList.GetNrIndices();
			for (uint32 i=0; i!=nrIndices; ++i)
			{
				uint64 blockIndex = freeList.GetFreeBlockIndex(i);
				EnsureThrowWithNr(blockIndex > 2, blockIndex);
				addIndex(blockIndex);
			}

			uint64 const nextIndex = freeList.GetPrevFreeListBlockIndex();
			if (UINT64_MAX == nextIndex)
				break;

			Rp<VarBlock> next = new VarBlock { nullptr };
			AfsResult::E r = next->ObtainBlock_CheckKind(m_storage, nextIndex, BlockKind::FreeList);
			EnsureThrow(AfsResult::OK == r);
			cur = std::move(next);
			++nrFullFreeListNodes;
		}

		EnsureThrowWithNr2(m_masterBlock->GetNrFullFreeListNodes() == nrFullFreeListNodes, m_masterBlock->GetNrFullFreeListNodes(), nrFullFreeListNodes);
	}


	AfsResult::E Afs::FindNameInDir(ObjId parentDirId, Seq name, DirEntry& entry)
	{
		EnsureThrow(State::Inited == m_state);

		DirCxR dcx { *this };
		DirNavPath navPath;
		return FindNameInDir_Inner(dcx, parentDirId, name, navPath, entry);
	}


	AfsResult::E Afs::CrackPath(Seq absPath, Vec<DirEntry>& entries)
	{
		EnsureThrow(State::Inited == m_state);

		entries.Clear();
		
		Seq reader = absPath;
		if (!reader.StripPrefixExact("/"))
			return AfsResult::InvalidPathSyntax;

		Vec<Seq> names;
		while (reader.n)
		{
			Seq name = reader.ReadToByte('/');
			if (!name.n)
				return AfsResult::InvalidPathSyntax;

			names.Add(name);
			reader.StripPrefixExact("/");
		}

		if (names.Any())
		{
			entries.ReserveExact(names.Len());

			ObjId parentDirId = ObjId::Root;
			for (Seq name : names)
			{
				if (entries.Any())
				{
					DirEntry& parent = entries.Last();
					if (ObjType::Dir != parent.m_type)
						return AfsResult::ObjNotDir;
					parentDirId = parent.m_id;
				}

				DirEntry entry;
				AfsResult::E r = FindNameInDir(parentDirId, name, entry);
				if (AfsResult::OK != r)
					return r;

				entries.Add(std::move(entry));
			}
		}

		return AfsResult::OK;
	}


	AfsResult::E Afs::ObjStat(ObjId id, StatInfo& info)
	{
		EnsureThrow(State::Inited == m_state);

		Rp<VarBlock> block = new VarBlock { nullptr };
		AfsResult::E r = GetTopBlock(block.Ref(), id, ObjType::Any);
		if (AfsResult::OK != r)
			return r;

		NodeView node = block->AsNodeView();
		TopView top = node.AsTopView();

		info.m_type = (ObjType::E) node.GetObjType();
		info.m_id = id;
		info.m_parentId = top.GetParentId();
		
		     if (ObjType::Dir  == info.m_type) info.m_dir_nrEntries  = top.Get_Dir_NrEntries();
		else if (ObjType::File == info.m_type) info.m_file_sizeBytes = top.Get_File_SizeBytes();

		info.m_createTime = Time::FromFt(top.GetCreateTime());
		info.m_modifyTime = Time::FromFt(top.GetModifyTime());
		info.m_metaData = top.GetMetaData();
		return AfsResult::OK;
	}


	AfsResult::E Afs::ObjSetStat(ObjId id, StatInfo const& info, uint32 statFields)
	{
		EnsureThrow(State::Inited == m_state);

		JournaledWrite jw { *this };
		try
		{
			Rp<VarBlock> block = new VarBlock { &jw };
			AfsResult::E r = GetTopBlock(block.Ref(), id, ObjType::Any);
			if (AfsResult::OK != r)
				return r;

			if (0 != statFields)
			{
				TopView top = block->AsNodeView().AsTopView();
				if (0 != (StatField::CreateTime & statFields)) top.SetCreateTime() = info.m_createTime.ToFt();
				if (0 != (StatField::ModifyTime & statFields)) top.SetModifyTime() = info.m_modifyTime.ToFt();

				if (0 != (StatField::MetaData & statFields))
				{
					if (info.m_metaData.Len() != top.GetMetaDataLen())
						return AfsResult::MetaDataCannotChangeLen;

					top.SetMetaData(info.m_metaData);
				}
			}

			jw.Complete();
			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::ObjDelete(ObjId parentDirId, Seq name, Time now)
	{
		EnsureThrow(State::Inited == m_state);

		ObjId objId;
		AfsResult::E r = ObjDelete_Inner(parentDirId, name, now, objId);
		if (AfsResult::FileNotEmpty == r)
		{
			uint64 actualNewSize {};
			r = FileSetSize_Inner(objId, 0, actualNewSize, now);
			if (AfsResult::OK == r)
				r = ObjDelete_Inner(parentDirId, name, now, objId);
		}
		return r;
	}


	AfsResult::E Afs::ObjMove(ObjId parentDirIdOld, Seq nameOld, ObjId parentDirIdNew, Seq nameNew, Time now)
	{
		EnsureThrow(State::Inited == m_state);

		AfsResult::E r = CheckName_Inner(nameNew);
		if (AfsResult::OK != r)
			return r;

		JournaledWrite jw { *this };
		try
		{
			DirCxRW dcxOld { *this, jw };
			r = dcxOld.GetDirTopBlock(parentDirIdOld);
			if (AfsResult::OK != r)
				return r;

			DirNavPath navPathOld;
			FindResult frOld = dcxOld.NavToLeafEntryEqualOrLessThan(navPathOld, nameOld, StopEarly::IfCantFind);
			if (FindResult::FoundEqual != frOld)
				return AfsResult::NameNotInDir;

			DirLeafEntry entry = dcxOld.GetLeafEntryAt(navPathOld);
			Opt<DirCxRW> dcxNewObj;
			DirCxRW* dcxNew;
			StopEarly stopEarly;

			if (parentDirIdNew == parentDirIdOld)
			{
				stopEarly = StopEarly::IfCantFind;
				dcxNew = &dcxOld;
			}
			else
			{
				if (parentDirIdNew == entry.m_id)
					return AfsResult::MoveDestInvalid;

				stopEarly = StopEarly::No;
				dcxNewObj.Init(*this, jw);
				dcxNew = dcxNewObj.Ptr();

				r = dcxNew->GetDirTopBlock(parentDirIdNew);
				if (AfsResult::OK != r)
					return r;

				if (ObjType::Dir == entry.m_type)
				{
					// Check to avoid creating a circular reference
					ObjId ancestorId = dcxNew->m_topNode->m_block->AsNodeView().AsTopView().GetParentId();
					while (ObjId::None != ancestorId && ObjId::Root != ancestorId && parentDirIdOld != ancestorId)
					{
						if (ancestorId == entry.m_id)
							return AfsResult::MoveDestInvalid;

						Rp<VarBlock> ancestor = new VarBlock { nullptr };
						r = GetTopBlock(ancestor.Ref(), ancestorId, ObjType::Dir);
						if (AfsResult::OK != r)
							return r;

						ancestorId = ancestor->AsNodeView().AsTopView().GetParentId();
					}
				}
			}

			DirNavPath navPathNew;
			FindResult frNew = dcxNew->NavToLeafEntryEqualOrLessThan(navPathNew, nameNew, stopEarly);
			if (FindResult::FoundEqual == frNew)
				return AfsResult::NameExists;

			dcxOld.RemoveLeafEntryAt(navPathOld, entry.m_id, now);

			entry.m_name = nameNew;

			if (!dcxNewObj.Any())
				dcxNew->AddLeafEntry(entry, now);
			else
			{
				if (FindResult::FoundLessThan == frNew)
					++(navPathNew.Last().m_pos);
				dcxNew->AddLeafEntryAt(entry, navPathNew, now, CanAddNode::Yes);
			}

			Rp<VarBlock> objBlock = new VarBlock { &jw };
			EnsureThrow(AfsResult::OK == GetTopBlock(objBlock.Ref(), entry.m_id, ObjType::Any));

			TopView top = objBlock->AsNodeView().AsTopView();
			EnsureThrow(top.GetParentId() == parentDirIdOld);
			top.SetParentId(parentDirIdNew);

			dcxOld.Finalize();
			if (dcxNewObj.Any())
				dcxNew->Finalize();

			jw.Complete();
			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::DirCreate(ObjId parentDirId, Seq name, Seq metaData, Time now, ObjId& dirId)
	{
		EnsureThrow(State::Inited == m_state);
		
		AfsResult::E r = CheckName_Inner(name);
		if (AfsResult::OK != r)
			return r;

		if (metaData.n > m_maxMetaBytes)
			return AfsResult::MetaDataTooLong;

		JournaledWrite jw { *this };
		try
		{
			DirCxRW dcx { *this, jw };
			r = dcx.GetDirTopBlock(parentDirId);
			if (AfsResult::OK != r)
				return r;

			DirNavPath navPath;
			FindResult fr = dcx.NavToLeafEntryEqualOrLessThan(navPath, name, StopEarly::No);
			if (FindResult::FoundEqual == fr)
				return AfsResult::NameExists;
			if (FindResult::FoundLessThan == fr)
				++(navPath.Last().m_pos);

			Rp<VarBlock> topBlock = jw.ReclaimBlockOrAddNew(BlockKind::Node);
			NodeView node = topBlock->AsNodeView();
			node.SetNodeCat() = NodeCat::Top;
			node.SetObjType() = ObjType::Dir;
		
			uint64 uniqueId = m_masterBlock->GetNextUniqueId();
			++(m_masterBlock->SetNextUniqueId());

			TopView top = node.AsTopView();
			top.SetUniqueId() = uniqueId;
			top.SetParentId(parentDirId);
			top.Set_Dir_NrEntries() = 0;
			top.SetCreateTime() = now.ToFt();
			top.SetModifyTime() = now.ToFt();
			top.SetMetaData(metaData);

			DirView dirView = top.AsDirView();
			dirView.SetDirNodeLevel() = 0;

			DirLeafView dirLeafView = dirView.AsLeafView();
			dirLeafView.SetNrEntries() = 0;
		
			DirLeafEntry entry;
			entry.m_id = top.GetObjId();
			entry.m_name = name;
			entry.m_type = ObjType::Dir;

			dcx.AddLeafEntryAt(entry, navPath, now, CanAddNode::Yes);
			dcx.Finalize();
			jw.Complete();

			dirId = entry.m_id;
			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::DirRead(ObjId dirId, Seq lastNameRead, Vec<DirEntry>& entries, bool& reachedEnd)
	{
		EnsureThrow(State::Inited == m_state);

		DirCxR dcx { *this };
		AfsResult::E r = dcx.GetDirTopBlock(dirId);
		if (AfsResult::OK != r)
			return r;

		DirNavPath navPath;
		FindResult fr = dcx.NavToLeafEntryEqualOrLessThan(navPath, lastNameRead, StopEarly::No);
		if (FindResult::FoundEqual == fr || FindResult::FoundLessThan == fr)
			++(navPath.Last().m_pos);

		bool reserveMore = true;
		uint32 nrAdvancements {};
		while (true)
		{
			DirNavEntry* navEntry = &navPath.Last();
			EnsureThrow(nullptr != navEntry->m_node);
			DirNode* node = navEntry->m_node;
			EnsureThrowWithNr(0 == node->m_level, node->m_level);
			EnsureThrowWithNr2(navEntry->m_pos <= node->m_leafEntries.Len(), navEntry->m_pos, node->m_leafEntries.Len());

			if (navEntry->m_pos == node->m_leafEntries.Len())
			{
				// Limit the number of advancements to correlate with logarithm of the number of directory entries
				if (nrAdvancements++ > dcx.m_topNode->m_level)
					break;

				// Advance to next node
				if (!dcx.NavToSiblingNode(navPath, EnumDir::Forward))
				{
					reachedEnd = true;
					break;
				}

				navEntry = &navPath.Last();
				EnsureThrow(nullptr != navEntry->m_node);
				node = navEntry->m_node;
				EnsureThrowWithNr(0 == node->m_level, node->m_level);
				EnsureThrowWithNr(0 == navEntry->m_pos, navEntry->m_pos);
				EnsureThrow(0 != node->m_leafEntries.Len());

				reserveMore = true;
			}

			if (reserveMore)
				entries.ReserveInc(node->m_leafEntries.Len() - navEntry->m_pos);

			DirLeafEntry const& leafEntry = node->m_leafEntries[navEntry->m_pos];
			DirEntry& dirEntry = entries.Add();
			dirEntry.m_id = leafEntry.m_id;
			dirEntry.m_type = leafEntry.m_type;
			dirEntry.m_name = leafEntry.m_name;

			++(navEntry->m_pos);
		}

		return AfsResult::OK;
	}


	AfsResult::E Afs::FileCreate(ObjId parentDirId, Seq name, Seq metaData, Time now, ObjId& fileId)
	{
		EnsureThrow(State::Inited == m_state);
		
		AfsResult::E r = CheckName_Inner(name);
		if (AfsResult::OK != r)
			return r;

		if (metaData.n > m_maxMetaBytes)
			return AfsResult::MetaDataTooLong;

		JournaledWrite jw { *this };
		try
		{
			DirCxRW dcx { *this, jw };
			r = dcx.GetDirTopBlock(parentDirId);
			if (AfsResult::OK != r)
				return r;

			DirNavPath navPath;
			FindResult fr = dcx.NavToLeafEntryEqualOrLessThan(navPath, name, StopEarly::No);
			if (FindResult::FoundEqual == fr)
				return AfsResult::NameExists;
			if (FindResult::FoundLessThan == fr)
				++(navPath.Last().m_pos);

			Rp<VarBlock> topBlock = jw.ReclaimBlockOrAddNew(BlockKind::Node);
			NodeView node = topBlock->AsNodeView();
			node.SetNodeCat() = NodeCat::Top;
			node.SetObjType() = ObjType::File;
		
			uint64 uniqueId = m_masterBlock->GetNextUniqueId();
			++(m_masterBlock->SetNextUniqueId());

			TopView top = node.AsTopView();
			top.SetUniqueId() = uniqueId;
			top.SetParentId(parentDirId);
			top.Set_File_SizeBytes() = 0;
			top.SetCreateTime() = now.ToFt();
			top.SetModifyTime() = now.ToFt();
			top.SetMetaData(metaData);

			FileView fileView = top.AsFileView();
			fileView.SetFileNodeLevel() = NodeLevel_BeyondMax;
		
			DirLeafEntry entry;
			entry.m_id = top.GetObjId();
			entry.m_name = name;
			entry.m_type = ObjType::File;

			dcx.AddLeafEntryAt(entry, navPath, now, CanAddNode::Yes);
			dcx.Finalize();
			jw.Complete();

			fileId = entry.m_id;
			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::FileMaxMiniNodeBytes(ObjId fileId, uint32& maxMiniNodeBytes)
	{
		EnsureThrow(State::Inited == m_state);

		FileCxR fcx { *this };
		AfsResult::E r = fcx.GetFileTopBlock(fileId);
		if (AfsResult::OK != r)
			return r;

		maxMiniNodeBytes = fcx.m_topNode->m_block->AsNodeView().AsTopView().AsFileView().AsIf_MiniView().ViewLen();
		return AfsResult::OK;
	}


	AfsResult::E Afs::FileRead(ObjId fileId, uint64 offset, sizet n, std::function<void(Seq, bool reachedEnd)> onData)
	{
		EnsureThrow(State::Inited == m_state);
		
		FileCxR fcx { *this };
		AfsResult::E r = fcx.GetFileTopBlock(fileId);
		if (AfsResult::OK != r)
			return r;

		TopView top = fcx.m_topNode->m_block->AsNodeView().AsTopView();
		uint64 const fileSize = top.Get_File_SizeBytes();

		if (offset > fileSize)
			return AfsResult::InvalidOffset;

		if (offset + n > fileSize)
			n = (sizet) (fileSize - offset);

		if (!n)
		{
			onData(Seq(), true);
			return AfsResult::OK;
		}

		bool reachedEnd = (offset + n == fileSize);
			
		FileView fileView = top.AsFileView();
		if (fileView.IsMini())
		{
			FileMiniView mini = fileView.AsMiniView();
			EnsureThrowWithNr2(offset + n <= mini.ViewLen(), (offset + n), mini.ViewLen());
			Seq data { mini.ReadPtr() + offset, n };
			onData(data, reachedEnd);
		}
		else
		{
			RpVec<VarBlock> blocks;
			fcx.GetDataBlocks(offset, offset + n, blocks);

			auto processBlock = [] (VarBlock& block, sizet offset, sizet offsetEnd, std::function<void(Seq, bool reachedEnd)> onData, bool reachedEnd)
				{
					EnsureThrowWithNr2(offset < offsetEnd, offset, offsetEnd);
					Seq data { block.ReadPtr() + offset, offsetEnd - offset };
					onData(data, reachedEnd);
				};

			if (1 == blocks.Len())
			{
				sizet blockOffset = (sizet) (offset % m_blockSize);
				processBlock(blocks.First().Ref(), blockOffset, blockOffset + n, onData, reachedEnd);
			}
			else
			{
				EnsureThrow(blocks.Any());
				processBlock(blocks.First().Ref(), (sizet) (offset % m_blockSize), m_blockSize, onData, false);

				sizet i;
				for (i=1; i+1 != blocks.Len(); ++i)
					processBlock(blocks[i].Ref(), 0, m_blockSize, onData, false);

				sizet offsetEnd = ((sizet) ((offset + (n - 1)) % m_blockSize)) + 1;
				processBlock(blocks.Last().Ref(), 0, offsetEnd, onData, reachedEnd);
			}
		}

		return AfsResult::OK;
	}


	AfsResult::E Afs::FileWrite(ObjId fileId, uint64 offset, Seq data, Time now)
	{
		EnsureThrow(State::Inited == m_state);

		JournaledWrite jw { *this };
		try
		{
			FileCxRW fcx { *this, jw };
			AfsResult::E r = fcx.GetFileTopBlock(fileId);
			if (AfsResult::OK != r)
				return r;

			TopView top = fcx.m_topNode->m_block->AsNodeView().AsTopView();
			uint64 const sizeRequired = offset + data.n;
			if (top.Get_File_SizeBytes() < sizeRequired)
				fcx.EnlargeToSize(sizeRequired);

			if (data.n)
			{
				EnsureThrowWithNr2(sizeRequired <= top.Get_File_SizeBytes(), sizeRequired, top.Get_File_SizeBytes());

				FileView fileView = top.AsFileView();
				if (fileView.IsMini())
				{
					FileMiniView mini = fileView.AsMiniView();
					EnsureThrowWithNr2(sizeRequired <= mini.ViewLen(), sizeRequired, mini.ViewLen());
					Mem::Copy<byte>(mini.WritePtr() + offset, data.p, data.n);
				}
				else
				{
					RpVec<VarBlock> blocks;
					fcx.GetDataBlocks(offset, offset + data.n, blocks);
					EnsureThrow(blocks.Any());

					auto writeBlock = [] (VarBlock& block, sizet offset, sizet blockSize, Seq& reader)
						{
							EnsureThrowWithNr2(offset < blockSize, offset, blockSize);
							Seq chunk = reader.ReadBytes(blockSize - offset);
							Mem::Copy<byte>(block.WritePtr() + offset, chunk.p, chunk.n);
						};

					Seq reader = data;
					writeBlock(blocks.First().Ref(), (sizet) (offset % m_blockSize), m_blockSize, reader);

					for (sizet i=1; i!=blocks.Len(); ++i)
					{
						EnsureThrow(0 != reader.n);
						writeBlock(blocks[i].Ref(), 0, m_blockSize, reader);
					}

					EnsureThrowWithNr(0 == reader.n, reader.n);
				}
			}

			top.SetModifyTime() = now.ToFt();
			fcx.Finalize();
			jw.Complete();

			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::FileSetSize(ObjId fileId, uint64 newSizeBytes, uint64& actualNewSize, Time now)
	{
		EnsureThrow(State::Inited == m_state);
		return FileSetSize_Inner(fileId, newSizeBytes, actualNewSize, now);
	}



	// Afs: private implementation

	AfsResult::E Afs::GetTopBlock(VarBlock& block, ObjId id, ObjType::E expectType)
	{
		AfsResult::E const notFoundResult = (ObjType::Dir == expectType ? AfsResult::DirNotFound : AfsResult::ObjNotFound);

		AfsResult::E r = block.ObtainBlock_CheckKind(m_storage, id.m_index, BlockKind::Node);
		if (AfsResult::StorageInErrorState == r) return AfsResult::StorageInErrorState;
		if (AfsResult::BlockIndexInvalid   == r) return AfsResult::InvalidObjId;		// "id" could not have been valid
		if (AfsResult::UnexpectedBlockKind == r) return notFoundResult;					// "id" could have referred to intended dir/file, but does not any more
		EnsureThrowWithNr(AfsResult::OK == r, r);										// Unexpected result from ObtainTyped()

		NodeView node = block.AsNodeView();
		if (node.GetNodeCat() != NodeCat::Top) return notFoundResult;					// "id" could have referred to intended dir/file, but does not any more
		TopView top = node.AsTopView();
		if (top.GetUniqueId() != id.m_uniqueId) return notFoundResult;					// "id" could have referred to intended dir/file, but does not any more

		     if (ObjType::Dir  == expectType) { if (node.GetObjType() != ObjType::Dir ) return AfsResult::ObjNotDir;  }
		else if (ObjType::File == expectType) { if (node.GetObjType() != ObjType::File) return AfsResult::ObjNotFile; }
		return AfsResult::OK;
	}


	template <class EntryType>
	Afs::FindResult Afs::FindNameEqualOrLessThan(Vec<EntryType> const& entries, Seq name, sizet& foundPos)
	{
		if (!entries.Any())
			{ foundPos = 0; return FindResult::NoEntries; }

		for (sizet i=0; i!=entries.Len(); ++i)
		{
			int d = m_cmp(entries[i].m_name, name);
			if (d == 0) { foundPos = i; return FindResult::FoundEqual; }
			if (d > 0)
			{
				if (i == 0) { foundPos = 0;     return FindResult::FirstIsGreater; }
				else        { foundPos = i - 1; return FindResult::FoundLessThan; }
			}
		}

		foundPos = entries.Len() - 1;
		return FindResult::FoundLessThan;
	}


	AfsResult::E Afs::CheckName_Inner(Seq name) const
	{
		if (name.n > m_maxNameBytes) return AfsResult::NameTooLong;
		if (name.ContainsByte('/')) return AfsResult::NameInvalid;
		return AfsResult::OK;
	}


	AfsResult::E Afs::FindNameInDir_Inner(DirCxR& dcx, ObjId parentDirId, Seq name, DirNavPath& navPath, DirEntry& entry)
	{
		AfsResult::E r = dcx.GetDirTopBlock(parentDirId);
		if (AfsResult::OK != r)
			return r;

		FindResult fr = dcx.NavToLeafEntryEqualOrLessThan(navPath, name, StopEarly::IfCantFind);
		if (FindResult::FoundEqual != fr)
			return AfsResult::NameNotInDir;

		DirLeafEntry& leafEntry = dcx.GetLeafEntryAt(navPath);
		entry.m_id = leafEntry.m_id;
		entry.m_type = leafEntry.m_type;
		entry.m_name = leafEntry.m_name;
		return AfsResult::OK;
	}


	AfsResult::E Afs::ObjDelete_Inner(ObjId parentDirId, Seq name, Time now, ObjId& objId)
	{
		JournaledWrite jw { *this };
		try
		{
			DirCxRW dcx { *this, jw };
			DirNavPath navPath;
			DirEntry entry;
			AfsResult::E r = FindNameInDir_Inner(dcx, parentDirId, name, navPath, entry);
			if (AfsResult::OK != r)
				return r;

			objId = entry.m_id;

			// If the object is a directory, verify that it is empty
			Rp<VarBlock> objBlock = new VarBlock { &jw };
			r = GetTopBlock(objBlock.Ref(), entry.m_id, ObjType::Any);
			EnsureThrowWithNr(AfsResult::OK == r, r);

			NodeView node = objBlock->AsNodeView();
			if (ObjType::Dir == node.GetObjType())
			{
				if (node.AsAnyDirView().Any())
					return AfsResult::DirNotEmpty;
			}
			else if (ObjType::File == node.GetObjType())
			{
				if (!node.AsAnyFileView().IsMini())
					return AfsResult::FileNotEmpty;
			}

			// We can delete the object
			jw.AddBlockToFree(objBlock);

			// Remove leaf entry from parent directory
			// This has possible cascade effects that can change and/or join parent directory nodes
			// This may result in blocks to be freed, but if a longer name takes the place of a shorter name
			// in one or more directory branch nodes, this can also require more storage
			dcx.RemoveLeafEntryAt(navPath, objId, now);
			dcx.Finalize();

			jw.Complete();
			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}


	AfsResult::E Afs::FileSetSize_Inner(ObjId fileId, uint64 newSizeBytes, uint64& actualNewSize, Time now)
	{
		try
		{
			actualNewSize = SIZE_MAX;

			bool lastRound {};
			do
			{
				JournaledWrite jw { *this };
				FileCxRW fcx { *this, jw };
				AfsResult::E r = fcx.GetFileTopBlock(fileId);
				if (AfsResult::OK != r)
					return r;

				TopView top = fcx.m_topNode->m_block->AsNodeView().AsTopView();
				uint64 const prevSizeBytes = top.Get_File_SizeBytes();
				actualNewSize = prevSizeBytes;

				if (prevSizeBytes == newSizeBytes)
				{
					top.SetModifyTime() = now.ToFt();		
					jw.Complete();
					break;
				}

				if (prevSizeBytes > newSizeBytes)
				{
					uint64 targetSize;
					if ((prevSizeBytes - newSizeBytes) / m_blockSize <= FileSetSize_MaxBlocksPerRound)
						{ targetSize = newSizeBytes; lastRound = true; }
					else
						targetSize = fcx.ImpliedCapacity(prevSizeBytes - (m_blockSize * FileSetSize_MaxBlocksPerRound));
					
					fcx.ShrinkToSize(targetSize);
					actualNewSize = targetSize;
				}
				else
				{
					uint64 targetSize;
					if ((newSizeBytes - prevSizeBytes) / m_blockSize <= FileSetSize_MaxBlocksPerRound)
						{ targetSize = newSizeBytes; lastRound = true; }
					else
						targetSize = fcx.ImpliedCapacity(prevSizeBytes + (m_blockSize * FileSetSize_MaxBlocksPerRound));

					fcx.EnlargeToSize(targetSize);
					actualNewSize = targetSize;

					FileView fileView = top.AsFileView();
					if (fileView.IsMini())
					{
						FileMiniView mini = fileView.AsMiniView();
						EnsureThrowWithNr2(prevSizeBytes <= mini.ViewLen(), prevSizeBytes, mini.ViewLen());
						EnsureThrowWithNr2(targetSize <= mini.ViewLen(), targetSize, mini.ViewLen());
						uint32 const prevSize32 = (uint32) prevSizeBytes;
						uint32 const targetSize32 = (uint32) targetSize;
						uint32 nrToZero = targetSize32 - prevSize32;
						Mem::Zero<byte>(mini.WritePtr() + prevSize32, nrToZero);
					}
					else
					{
						RpVec<VarBlock> blocks;
						fcx.GetDataBlocks(prevSizeBytes, targetSize, blocks);
						EnsureThrow(blocks.Any());

						auto zeroBlock = [] (VarBlock& block, sizet offset, sizet blockSize, uint64& nToGo)
							{
								EnsureThrowWithNr2(offset < blockSize, offset, blockSize);

								sizet nThisBlock = blockSize - offset;
								if (nToGo > nThisBlock)
									nToGo -= nThisBlock;
								else
									nToGo = 0;

								Mem::Zero<byte>(block.WritePtr() + offset, nThisBlock);
							};

						uint64 nToGo = targetSize - prevSizeBytes;
						zeroBlock(blocks.First().Ref(), (sizet) (prevSizeBytes % m_blockSize), m_blockSize, nToGo);

						for (sizet i=1; i!=blocks.Len(); ++i)
						{
							EnsureThrow(0 != nToGo);
							zeroBlock(blocks[i].Ref(), 0, m_blockSize, nToGo);
						}

						EnsureThrowWithNr(0 == nToGo, nToGo);
					}
				}

				top.SetModifyTime() = now.ToFt();		
				fcx.Finalize();
				jw.Complete();
			}
			while (!lastRound);

			return AfsResult::OK;
		}
		catch (AfsResult::E r) { return r; }
	}

}
