#pragma once

#include "AtAfs.h"

#include "AtMap.h"


namespace At
{

	// Partially abstract cryptographic layer that fits between Afs and underlying storage, such as AfsFileStorage.
	// Provides virtual functions which a derived class must override to implement actual cryptography.
	// Currently requires a hash function with 512-bit output (e.g. SHA-512), a MAC with 512-bit output (e.g. HMAC-SHA-512),
	// and a cipher that accepts a 256-bit key, 128-bit IV, and input/output sizes that are multiples of 128 bits (e.g. AES256-CBC).

	class AfsCryptStorage : public AfsStorage
	{
	public:
		enum
		{
			MinOuterBlockSize = 512,
			EncrKeyBytes      = 32,
			MacKeyBytes       = 64,
		};

		// Pass the underlying storage, for example an instance of AfsFileStorage
		void SetOuterStorage(AfsStorage& storage);

		// If the underlying storage has not yet been used, initializes it for access using the provided accessEncrKey and accessMacKey.
		// If the underlying storage has been so initialized, attempts to access it using the provided accessEncrKey and accessMacKey.
		// If the MAC key is incorrect, returns false. Otherwise, throws an exception on error.
		// accessEncrKey and accessMacKey must be already derived. Their lengths must equal EncrKeyBytes and MacKeyBytes, respectively.
		bool Init(Seq accessEncrKey, Seq accessMacKey);

	public:
		BlockAllocator& Allocator() override final;
		uint32 BlockSize() override final;
		uint64 MaxNrBlocks() override final;
		uint64 NrBlocks() override final;
		AfsResult::E AddNewBlock(AfsBlock& block) override final;
		AfsResult::E ObtainBlock(AfsBlock& block, uint64 blockIndex) override final;
		AfsResult::E ObtainBlockForOverwrite(AfsBlock& block, uint64 blockIndex) override final;
		void BeginJournaledWrite() override final;
		void AbortJournaledWrite() noexcept override final;
		void CompleteJournaledWrite(RpVec<AfsBlock> const& blocksToWrite) override final;

	protected:
		enum
		{
			HashDigestBytes  = 64,
			CipherBlockBytes = 16,
		};

		virtual void ACS_Init() = 0;

		virtual void ACS_Random_GenBytes(byte* out, uint outBytes) = 0;

		virtual void ACS_Hash_Begin() = 0;
		virtual void ACS_Hash_Update(Seq in) = 0;
		virtual void ACS_Hash_Final(byte* out, uint outBytes) = 0;

		// "macKey" will be MacKeyBytes in length. Output buffer will be of size HashDigestBytes.
		virtual void ACS_Hmac_Process(Seq macKey, Seq in, byte* out, uint outBytes) = 0;

		// "encrKey" will be EncrKeyBytes in length. "iv" will be CipherBlockBytes in length.
		// Input is of a size that's a multiple of CipherBlockBytes.
		// Output buffer will have the same size as input, and expects to receive the same number of bytes.
		virtual void ACS_Cipher_Process(Seq encrKey, Seq iv, Seq in, byte* out, uint outBytes, EncrDir encrDir) = 0;

	private:

		struct InternalWrite : AfsChangeTracker
		{
			InternalWrite(AfsStorage& storage) : m_storage(storage) { m_storage.BeginJournaledWrite(); }
			InternalWrite(InternalWrite&&) noexcept = default;
			~InternalWrite();

			void Complete();

		private:
			AfsStorage& m_storage;
			bool        m_completed {};
		};


		enum class State { Uninited, Ready, JournaledWrite };

		enum
		{
			KeyBlockPrefixBytes     = 16,
			KeyBlockSignature1      = 0x43736641,	// "AfsC" (little-endian)
			KeyBlockSignature2      = 0x74707972,	// "rypt" (little-endian)
			KeyBlockPrefixVersion   = 0,
			KeyBlockPayloadVersion  = 0,
			MasterSecretBytes       = 32,
			KeyBlockPayloadBytes    = 8 + MasterSecretBytes,
			KeyBlockNonPayloadBytes = KeyBlockPrefixBytes + CipherBlockBytes + HashDigestBytes,

			BlockSaltBytes          = 16,
			BlockMacBytes           = 32,
		};

		State           m_state           {};
		AfsStorage*     m_storage         {};
		BlockAllocator  m_allocator;
		Str             m_keyBlockPayload;
		Seq             m_masterSecret;
		uint32          m_outerBlockSize  {};
		uint32          m_innerBlockSize  {};
		uint64          m_nrInnerBlocks   {};
		size_t          m_nrBlocksToAdd   {};


		void CalcBlockMac(uint64 blockIndex, Seq blockSalt, Seq ciphertext, byte* out, uint32 outBytes);
		void ProcessBlock(uint64 blockIndex, Seq blockSalt, Seq input, byte* out, uint32 outBytes, EncrDir encrDir);
	};

}
