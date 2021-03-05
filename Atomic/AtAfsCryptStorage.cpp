#include "AtIncludes.h"
#include "AtAfsCryptStorage.h"

#include "AtEncode.h"
#include "AtException.h"


namespace At
{

	// AfsCryptStorage::InternalWrite

	AfsCryptStorage::InternalWrite::~InternalWrite()
	{
		if (!m_completed)
		{
			m_storage.AbortJournaledWrite();
			for (Rp<AfsBlock> const& block : m_changedBlocks)
				block->OnWriteAborted();
		}
	}


	void AfsCryptStorage::InternalWrite::Complete()
	{
		EnsureThrow(!m_completed);

		m_storage.CompleteJournaledWrite(m_changedBlocks);
		m_completed = true;

		for (Rp<AfsBlock> const& block : m_changedBlocks)
			block->OnWriteComplete();
	}



	// AfsCryptStorage

	void AfsCryptStorage::SetOuterStorage(AfsStorage& storage)
	{
		EnsureThrow(State::Uninited == m_state);
		m_storage = &storage;
	}


	bool AfsCryptStorage::Init(Seq accessEncrKey, Seq accessMacKey)
	{
		EnsureThrow(State::Uninited == m_state);
		EnsureThrow(nullptr != m_storage);
		EnsureThrow(EncrKeyBytes == accessEncrKey.n);
		EnsureThrow(MacKeyBytes == accessMacKey.n);

		m_outerBlockSize = NumCast<uint32>(m_storage->BlockSize());
		if (m_outerBlockSize < MinOuterBlockSize)
			throw StrErr(Str(__FUNCTION__ ": Outer block size ").UInt(m_outerBlockSize).Add(" is too small, required: ").UInt(MinOuterBlockSize));
		if (0 != (m_outerBlockSize % CipherBlockBytes))
			throw StrErr(Str(__FUNCTION__ ": Outer block size ").UInt(m_outerBlockSize).Add(" is not a multiple of cipher block size ").UInt(CipherBlockBytes));

		m_innerBlockSize = m_outerBlockSize - (BlockSaltBytes + BlockMacBytes);
		EnsureThrow(m_innerBlockSize >= CipherBlockBytes);
		EnsureThrow(0 == (m_innerBlockSize % CipherBlockBytes));

		ACS_Init();

		uint32 const payloadBytes = m_outerBlockSize - KeyBlockNonPayloadBytes;
		m_keyBlockPayload.ResizeExact(payloadBytes);

		if (0 == m_storage->NrBlocks())
		{
			{
				byte*             pWrite = m_keyBlockPayload.Ptr();
				byte const* const pEnd   = m_keyBlockPayload.PtrEnd();

				pWrite = EncodeUInt32LE_Ptr(pWrite, KeyBlockPayloadVersion);
				pWrite = EncodeUInt32LE_Ptr(pWrite, m_innerBlockSize);

				ptrdiff const nrBytesRemaining = pEnd - pWrite;
				EnsureThrow(nrBytesRemaining >= MasterSecretBytes);
				m_masterSecret.Set(pWrite, MasterSecretBytes);
				ACS_Random_GenBytes(pWrite, (uint) nrBytesRemaining);
			}

			InternalWrite iw { *m_storage };
			Rp<AfsBlock> keyBlock = new AfsBlock { &iw };
			AfsResult::E r = m_storage->AddNewBlock(keyBlock.Ref());
			if (AfsResult::OK != r)
				throw StrErr(Str::Join(__FUNCTION__ ": Could not create key block: ", AfsResult::Name(r)));

			EnsureThrow(0 == keyBlock->BlockIndex());
			EnsureThrowWithNr2(keyBlock->BlockSize() == m_outerBlockSize, keyBlock->BlockSize(), m_outerBlockSize);

			{
				byte* pWrite = keyBlock->WritePtr();
				byte const* const pEnd = pWrite + m_outerBlockSize;

				pWrite = EncodeUInt32LE_Ptr(pWrite, KeyBlockSignature1);
				pWrite = EncodeUInt32LE_Ptr(pWrite, KeyBlockSignature2);
				pWrite = EncodeUInt32LE_Ptr(pWrite, 0);
				pWrite = EncodeUInt32LE_Ptr(pWrite, payloadBytes);

				ACS_Random_GenBytes(pWrite, CipherBlockBytes);
				Seq iv { pWrite, CipherBlockBytes };
				pWrite += CipherBlockBytes;

				ptrdiff nrBytesRemaining = pEnd - pWrite;
				EnsureThrow(payloadBytes + HashDigestBytes == nrBytesRemaining);
				ACS_Cipher_Process(accessEncrKey, iv, m_keyBlockPayload, pWrite, payloadBytes, EncrDir::Encrypt);
				pWrite += payloadBytes;

				Seq macData { keyBlock->WritePtr(), m_outerBlockSize - HashDigestBytes };
				ACS_Hmac_Process(accessMacKey, macData, pWrite, HashDigestBytes);
				pWrite += HashDigestBytes;
				EnsureAbort(pEnd == pWrite);
			}

			iw.Complete();
		}
		else
		{
			Rp<AfsBlock> keyBlock = new AfsBlock { nullptr };
			AfsResult::E r = m_storage->ObtainBlock(keyBlock.Ref(), 0);
			EnsureThrow(AfsResult::OK == r);
			EnsureThrow(0 == keyBlock->BlockIndex());
			EnsureThrow(keyBlock->BlockSize() == m_outerBlockSize);

			Seq outerReader { keyBlock->ReadPtr(), m_outerBlockSize };

			uint32 sig1, sig2, prefixVer, ciphertextLen;
			EnsureThrow(DecodeUInt32LE(outerReader, sig1));
			EnsureThrow(DecodeUInt32LE(outerReader, sig2));
			EnsureThrow(DecodeUInt32LE(outerReader, prefixVer));
			EnsureThrow(DecodeUInt32LE(outerReader, ciphertextLen));

			if (KeyBlockSignature1    != sig1)      throw StrErr(Str(__FUNCTION__ ": Unexpected key block signature 1: 0x").UInt(sig1, 16, 8));
			if (KeyBlockSignature2    != sig2)      throw StrErr(Str(__FUNCTION__ ": Unexpected key block signature 2: 0x").UInt(sig2, 16, 8));
			if (KeyBlockPrefixVersion != prefixVer) throw StrErr(Str(__FUNCTION__ ": Unexpected key block prefix version: 0x").UInt(prefixVer, 16, 8));

			if (payloadBytes != ciphertextLen)
				throw StrErr(Str(__FUNCTION__ ": Key block ciphertext length ").UInt(ciphertextLen)
					.Add(" does not match expected length ").UInt(payloadBytes));

			Seq iv         = outerReader.ReadBytes(CipherBlockBytes);
			Seq ciphertext = outerReader.ReadBytes(payloadBytes);
			Seq mac        = outerReader.ReadBytes(HashDigestBytes);
			EnsureThrow(HashDigestBytes == mac.n);
			EnsureThrow(0 == outerReader.n);

			Seq macData { keyBlock->ReadPtr(), m_outerBlockSize - HashDigestBytes };
			byte digest[HashDigestBytes];
			ACS_Hmac_Process(accessMacKey, macData, digest, HashDigestBytes);
			if (!mac.ConstTimeEqualExact(Seq(digest, HashDigestBytes)))
				return false;

			ACS_Cipher_Process(accessEncrKey, iv, ciphertext, m_keyBlockPayload.Ptr(), payloadBytes, EncrDir::Decrypt);

			Seq innerReader = m_keyBlockPayload;
			uint32 payloadVer, ibs;
			EnsureThrow(DecodeUInt32LE(innerReader, payloadVer));
			EnsureThrow(DecodeUInt32LE(innerReader, ibs));
			if (KeyBlockPayloadVersion != payloadVer) throw StrErr(Str(__FUNCTION__ ": Unrecognized key block payload version: ").UInt(payloadVer));
			if (m_innerBlockSize       != ibs)        StrErr(Str(__FUNCTION__ ": Inner block size ").UInt(ibs).Add(" does not match expected ").UInt(m_innerBlockSize));

			m_masterSecret = innerReader.ReadBytes(MasterSecretBytes);
			if (MasterSecretBytes != m_masterSecret.n)
				throw StrErr(Str(__FUNCTION__ ": Unexpected master secret size: ").UInt(m_masterSecret.n));
		}

		m_allocator.SetBytesPerBlock(m_innerBlockSize);

		uint64 const nrStorageBlocks = m_storage->NrBlocks();
		EnsureThrow(nrStorageBlocks >= 1);
		m_nrInnerBlocks = nrStorageBlocks - 1;
		m_state = State::Ready;
		return true;
	}


	BlockAllocator& AfsCryptStorage::Allocator()
	{
		EnsureThrow(State::Uninited != m_state);
		return m_allocator;
	}


	uint32 AfsCryptStorage::BlockSize()
	{
		EnsureThrow(State::Uninited != m_state);
		return m_innerBlockSize;
	}


	uint64 AfsCryptStorage::MaxNrBlocks()
	{
		EnsureThrow(State::Uninited != m_state);
		uint64 outerMax = m_storage->MaxNrBlocks();
		if (UINT64_MAX == outerMax) return UINT64_MAX;
		if (0 == outerMax) return 0;
		return outerMax - 1;
	}


	uint64 AfsCryptStorage::NrBlocks()
	{
		EnsureThrow(State::Uninited != m_state);
		return SatSub<uint64>(m_storage->NrBlocks(), 1);
	}


	AfsResult::E AfsCryptStorage::AddNewBlock(AfsBlock& block)
	{
		EnsureThrow(State::JournaledWrite == m_state);
		if (1 + m_nrInnerBlocks + m_nrBlocksToAdd >= m_storage->MaxNrBlocks())
			return AfsResult::OutOfSpace;

		block.Init(*this, m_nrInnerBlocks + m_nrBlocksToAdd, new RcBlock(m_allocator));
		++m_nrBlocksToAdd;
		return AfsResult::OK;
	}


	AfsResult::E AfsCryptStorage::ObtainBlock(AfsBlock& block, uint64 blockIndex)
	{
		EnsureThrow(State::Uninited != m_state);
		if (blockIndex >= m_nrInnerBlocks)
			return AfsResult::BlockIndexInvalid;

		EnsureThrow(UINT64_MAX != blockIndex);
		uint64 const outerIndex = 1 + blockIndex;
		Rp<AfsBlock> outerBlock = new AfsBlock { nullptr };
		AfsResult::E r = m_storage->ObtainBlock(outerBlock.Ref(), outerIndex);
		if (r != AfsResult::OK)
			return r;

		EnsureThrow(outerBlock->BlockIndex() == outerIndex);
		Seq reader { outerBlock->ReadPtr(), outerBlock->BlockSize() };
		Seq blockSalt  = reader.ReadBytes(BlockSaltBytes);
		Seq ciphertext = reader.ReadBytes(m_innerBlockSize);
		Seq mac        = reader.ReadBytes(BlockMacBytes);
		EnsureThrow(BlockMacBytes == mac.n);

		byte digest[HashDigestBytes];
		OnExit zeroDigest = [&] () { Mem::Zero<byte>(digest, HashDigestBytes); };
		CalcBlockMac(blockIndex, blockSalt, ciphertext, digest, HashDigestBytes);
		static_assert(BlockMacBytes <= HashDigestBytes, "");
		if (!mac.ConstTimeEqualExact(Seq(digest, BlockMacBytes)))
			throw StrErr(Str(__FUNCTION__ ": Invalid block MAC at inner block index ").UInt(blockIndex));

		Rp<RcBlock> dataBlock = new RcBlock { m_allocator };
		ProcessBlock(blockIndex, blockSalt, ciphertext, dataBlock->Ptr(), m_innerBlockSize, EncrDir::Decrypt);
		block.Init(*this, blockIndex, dataBlock);
		return AfsResult::OK;
	}


	AfsResult::E AfsCryptStorage::ObtainBlockForOverwrite(AfsBlock& block, uint64 blockIndex)
	{
		EnsureThrow(State::JournaledWrite == m_state);
		if (blockIndex >= m_nrInnerBlocks)
			return AfsResult::BlockIndexInvalid;

		block.Init(*this, blockIndex, new RcBlock(m_allocator));
		return AfsResult::OK;
	}


	void AfsCryptStorage::BeginJournaledWrite()
	{
		EnsureThrow(State::Ready == m_state);
		EnsureThrow(0 == m_nrBlocksToAdd);
		m_state = State::JournaledWrite;
	}


	void AfsCryptStorage::AbortJournaledWrite() noexcept
	{
		EnsureThrow(State::JournaledWrite == m_state);
		m_nrBlocksToAdd = 0;
		m_state = State::Ready;
	}


	void AfsCryptStorage::CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite)
	{
		EnsureThrow(State::JournaledWrite == m_state);
		EnsureThrow(blocksToWrite.Len() >= m_nrBlocksToAdd);

		if (blocksToWrite.Any())
		{
			InternalWrite iw { *m_storage };

			// Create new blocks in advance because blocksToWrite likely does not contain them in order
			RpVec<AfsBlock> blocksToAdd;
			blocksToAdd.ReserveExact(m_nrBlocksToAdd);
			for (size_t i=0; i!=m_nrBlocksToAdd; ++i)
			{
				Rp<AfsBlock> outerBlock = new AfsBlock { &iw };
				AfsResult::E r = m_storage->AddNewBlock(outerBlock.Ref());
				if (AfsResult::OutOfSpace == r) throw r;
				EnsureThrowWithNr(AfsResult::OK == r, r);

				blocksToAdd.Add(std::move(outerBlock));
			}

			// Initialize overwritten blocks as we go
			RpVec<AfsBlock> blocksToOverwrite;
			blocksToOverwrite.ReserveExact(blocksToWrite.Len() - m_nrBlocksToAdd);

			size_t nrBlocksAdded {}, nrBlocksOverwritten {};
			for (Rp<AfsBlock> block : blocksToWrite)
			{
				uint64 const blockIndex = block->BlockIndex();
				EnsureThrow(UINT64_MAX != blockIndex);
				uint64 const outerIndex = 1 + blockIndex;
		
				AfsBlock* outerBlockPtr {};
				if (blockIndex >= m_nrInnerBlocks)
				{
					EnsureThrow(blockIndex < m_nrInnerBlocks + m_nrBlocksToAdd);
					size_t blocksToAddIndex = (size_t) (blockIndex - m_nrInnerBlocks);
					outerBlockPtr = blocksToAdd[blocksToAddIndex].Ptr();
					EnsureThrow(outerBlockPtr->BlockIndex() == outerIndex);
					++nrBlocksAdded;
				}
				else
				{
					Rp<AfsBlock> outerBlock = new AfsBlock { &iw };
					AfsResult::E r = m_storage->ObtainBlockForOverwrite(outerBlock.Ref(), outerIndex);
					EnsureThrowWithNr(AfsResult::OK == r, r);
					outerBlockPtr = outerBlock.Ptr();
					blocksToOverwrite.Add(std::move(outerBlock));
					++nrBlocksOverwritten;
				}

				byte*             pWrite = outerBlockPtr->WritePtr();
				byte const* const pEnd   = pWrite + outerBlockPtr->BlockSize();

				ACS_Random_GenBytes(pWrite, BlockSaltBytes);
				Seq blockSalt { pWrite, BlockSaltBytes };
				pWrite += BlockSaltBytes;

				Seq plaintext { block->ReadPtr(), m_innerBlockSize };
				ProcessBlock(blockIndex, blockSalt, plaintext, pWrite, m_innerBlockSize, EncrDir::Encrypt);
				Seq ciphertext { pWrite, m_innerBlockSize };
				pWrite += m_innerBlockSize;

				byte digest[HashDigestBytes];
				CalcBlockMac(blockIndex, blockSalt, ciphertext, digest, HashDigestBytes);
				static_assert(BlockMacBytes <= HashDigestBytes, "");
				Mem::Copy<byte>(pWrite, digest, BlockMacBytes);
				pWrite += BlockMacBytes;
				EnsureAbort(pEnd == pWrite);
			}

			EnsureThrow(nrBlocksAdded == m_nrBlocksToAdd);
			EnsureThrow(nrBlocksAdded + nrBlocksOverwritten == blocksToWrite.Len());
			iw.Complete();
			m_nrInnerBlocks += m_nrBlocksToAdd;
		}

		m_nrBlocksToAdd = 0;
		m_state = State::Ready;
	}


	void AfsCryptStorage::CalcBlockMac(uint64 blockIndex, Seq blockSalt, Seq ciphertext, byte* out, uint32 outBytes)
	{
		EnsureThrow(BlockSaltBytes == blockSalt.n);
		Seq blockIndexSeq { (byte const*) &blockIndex, sizeof(blockIndex) };

		ACS_Hash_Begin();
		ACS_Hash_Update(m_masterSecret);
		ACS_Hash_Update(blockIndexSeq);
		ACS_Hash_Update(blockSalt);
		ACS_Hash_Update("MAC");
		ACS_Hash_Update(ciphertext);
		ACS_Hash_Final(out, outBytes);
	}


	void AfsCryptStorage::ProcessBlock(uint64 blockIndex, Seq blockSalt, Seq input, byte* out, uint32 outBytes, EncrDir encrDir)
	{
		EnsureThrow(BlockSaltBytes == blockSalt.n);
		Seq blockIndexSeq { (byte const*) &blockIndex, sizeof(blockIndex) };

		enum { BufBytes = HashDigestBytes };
		byte buf[BufBytes];
		OnExit zeroBuf = [&] () { ZeroMemory(buf, BufBytes); };

		ACS_Hash_Begin();
		ACS_Hash_Update(m_masterSecret);
		ACS_Hash_Update(blockIndexSeq);
		ACS_Hash_Update(blockSalt);
		ACS_Hash_Update("ENC");
		ACS_Hash_Final(buf, BufBytes);

		Seq reader { buf, BufBytes };
		Seq encrKey = reader.ReadBytes(EncrKeyBytes);
		Seq iv = reader.ReadBytes(CipherBlockBytes);
		EnsureThrow(CipherBlockBytes == iv.n);

		ACS_Cipher_Process(encrKey, iv, input, out, outBytes, encrDir);
	}

}
