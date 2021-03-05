#pragma once

#include "AtAfsCryptStorage.h"
#include "AtBCrypt.h"


namespace At
{

	// An instantiation of AfsCryptStorage using Windows CNG (bcrypt) to implement the cryptography

	class AfsBCryptStorage : public AfsCryptStorage
	{
	protected:
		void ACS_Init() override final;
		void ACS_Random_GenBytes(byte* out, uint outBytes) override final;
		void ACS_Hash_Begin() override final;
		void ACS_Hash_Update(Seq in) override final;
		void ACS_Hash_Final(byte* out, uint outBytes) override final;
		void ACS_Hmac_Process(Seq macKey, Seq in, byte* out, uint outBytes) override final;
		void ACS_Cipher_Process(Seq encrKey, Seq iv, Seq in, byte* out, uint outBytes, EncrDir encrDir) override final;

	private:
		BCrypt::Provider m_provRng;
		BCrypt::Provider m_provHash;
		BCrypt::Provider m_provHmac;
		BCrypt::Provider m_provCipher;

		BCrypt::Hash m_hash;
	};

}
