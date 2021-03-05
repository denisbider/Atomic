#include "AtIncludes.h"
#include "AtAfsBCryptStorage.h"

#include "AtAuto.h"


namespace At
{

	void AfsBCryptStorage::ACS_Init()
	{
		m_provRng.OpenRng();
		m_provHash.OpenSha512();
		m_provHmac.OpenHmacSha512();
		m_provCipher.OpenAes();

		m_hash.Init(m_provHash);
	}


	void AfsBCryptStorage::ACS_Random_GenBytes(byte* out, uint outBytes)
	{
		m_provRng.GenRandom(out, outBytes);
	}


	void AfsBCryptStorage::ACS_Hash_Begin()
	{
		m_hash.Begin(m_provHash);
	}


	void AfsBCryptStorage::ACS_Hash_Update(Seq in)
	{
		m_hash.Update(in);
	}


	void AfsBCryptStorage::ACS_Hash_Final(byte* out, uint outBytes)
	{
		m_hash.Final(out, outBytes);
	}


	void AfsBCryptStorage::ACS_Hmac_Process(Seq macKey, Seq in, byte* out, uint outBytes)
	{
		BCrypt::Hash hmac;
		hmac.Init(m_provHmac, macKey);
		hmac.Begin(m_provHmac);
		hmac.Update(in);
		hmac.Final(out, outBytes);
	}


	void AfsBCryptStorage::ACS_Cipher_Process(Seq encrKey, Seq iv, Seq in, byte* out, uint outBytes, EncrDir encrDir)
	{
		BCrypt::Key cipher;
		cipher.GenerateSymmetricKey(m_provCipher, encrKey);
		cipher.SetChainMode(BCRYPT_CHAIN_MODE_CBC);

		EnsureThrow(CipherBlockBytes == iv.n);
		byte ivCopy[CipherBlockBytes];
		AutoZeroMemory<byte, CipherBlockBytes> zeroIvCopy { ivCopy };
		memcpy(ivCopy, iv.p, CipherBlockBytes);

		if (EncrDir::Encrypt == encrDir)
			cipher.Encrypt_NoPadding(in, ivCopy, CipherBlockBytes, out, outBytes);
		else
			cipher.Decrypt_NoPadding(in, ivCopy, CipherBlockBytes, out, outBytes);
	}

}
