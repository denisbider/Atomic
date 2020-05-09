#include "AtIncludes.h"
#include "AtCrypt.h"

#include "AtAsn1.h"
#include "AtDllBCrypt.h"
#include "AtException.h"
#include "AtNumCvt.h"
#include "AtVec.h"
#include "AtWinErr.h"


// Global objects MUST be initialized before code that may call Init().
#pragma warning (push)
#pragma warning (disable: 4073)
#pragma init_seg (lib)
#pragma warning (pop)


namespace At
{

	namespace Crypt
	{

		// Globals
		Mutex             g_mxInit;
		sizet             g_initedCount {};
		HCRYPTPROV        g_cryptProv   {};
		BCRYPT_ALG_HANDLE g_phRsa       {};



		// Initialization

		void Init()
		{
			Locker locker { g_mxInit };

			if (!g_initedCount)
			{
				if (!CryptAcquireContextW(&g_cryptProv, 0, 0, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
					{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptAcquireContext"); }

				OnExit releaseContext = [&] { CryptReleaseContext(g_cryptProv, 0); };

				NTSTATUS st = Call_BCryptOpenAlgorithmProvider(&g_phRsa, BCRYPT_RSA_ALGORITHM, NULL, 0);
				if (st != STATUS_SUCCESS)
					throw NtStatusErr<>(st, __FUNCTION__ ": BCryptOpenAlgorithmProvider(BCRYPT_RSA_ALGORITHM)");

				releaseContext.Dismiss();
			}

			++g_initedCount;
		}


		void Free()
		{
			Locker locker { g_mxInit };

			EnsureAbort(g_initedCount >= 1);
			--g_initedCount;
			if (!g_initedCount)
			{
				EnsureAbort(Call_BCryptCloseAlgorithmProvider(g_phRsa, 0) == STATUS_SUCCESS);
				EnsureAbort(CryptReleaseContext(g_cryptProv, 0));
			}
		}


		bool Inited()
		{
			return g_initedCount >= 1;
		}



		// GenRandom

		void GenRandom(void* buffer, uint len)
		{
			EnsureThrow(g_initedCount != 0);
			if (len != 0)
				if (!CryptGenRandom(g_cryptProv, len, (BYTE*) buffer))
					{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGenRandom"); }
		}
		

		uint64 GenRandomNr(uint64 maxVal)
		{
			uint bits {};
			uint64 v { maxVal };
			while (v) { v >>= 1; ++bits; }

			do
			{
				v = 0;
				GenRandom(&v, (bits+7)/8);
				if (bits < 64)
					v &= (1ULL << bits) - 1;
			}
			while (v > maxVal);

			return v;
		}



		// CryptProtectData and UnprotectData

		void ProtectData(Enc& enc, Seq in, DWORD flags)
		{
			DATA_BLOB dbIn  { NumCast<DWORD>(in.n), (BYTE*) in.p };
			DATA_BLOB dbOut {};
			if (!CryptProtectData(&dbIn, nullptr, nullptr, nullptr, nullptr, flags, &dbOut))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptProtectData"); }

			OnExit freeOut = [&]
				{
					if (LocalFree(dbOut.pbData) != nullptr)
						if (!std::uncaught_exception())
							{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": LocalFree"); }
				};

			enc.Add(Seq(dbOut.pbData, dbOut.cbData));
		}

		
		void UnprotectData(Enc& enc, Seq in, DWORD flags)
		{
			DATA_BLOB dbIn  { NumCast<DWORD>(in.n), (BYTE*) in.p };
			DATA_BLOB dbOut {};
			if (!CryptUnprotectData(&dbIn, nullptr, nullptr, nullptr, nullptr, flags, &dbOut))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptUnprotectData"); }

			OnExit freeOut = [&]
				{
					if (LocalFree(dbOut.pbData) != nullptr)
						if (!std::uncaught_exception())
							{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": LocalFree"); }
				};

			enc.Add(Seq(dbOut.pbData, dbOut.cbData));
		}

	}	// namespace Crypt

	using namespace Crypt;



	// Hash

	Hash::~Hash()
	{
		try { Close(); }
		catch (Exception const& e) { EnsureFailWithDesc(OnFail::Report, e.what(), __FUNCTION__, __LINE__); }
	}


	Hash& Hash::Close()
	{
		if (m_haveHash)
		{
			m_haveHash = false;
			m_hashSize = 0;
		
			if (!CryptDestroyHash(m_hash))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptDestroyHash"); }
		}
	
		return *this;
	}


	Hash& Hash::Create(ALG_ID algId)
	{
		Close();
	
		if (!CryptCreateHash(g_cryptProv, algId, 0, 0, &m_hash))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptCreateHash"); }
		m_haveHash = true;
	
		InitHashSize();
	
		return *this;
	}


	Hash& Hash::CreateHmac(ALG_ID algId, HCRYPTKEY macKey)
	{
		Close();
	
		if (!CryptCreateHash(g_cryptProv, CALG_HMAC, macKey, 0, &m_hash))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptCreateHash(CALG_HMAC)"); }
		m_haveHash = true;
	
		HMAC_INFO hmacInfo {};
		hmacInfo.HashAlgid = algId;
		if (!CryptSetHashParam(m_hash, HP_HMAC_INFO, (BYTE*) &hmacInfo, 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptSetHashParam(HP_HMAC_INFO)"); }
	
		InitHashSize();
	
		return *this;
	}


	void Hash::InitHashSize()
	{
		EnsureThrow(m_haveHash);
	
		DWORD dataLen = sizeof(m_hashSize);
		if (!CryptGetHashParam(m_hash, HP_HASHSIZE, (BYTE*) &m_hashSize, &dataLen, 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGetHashParam(HP_HASHSIZE)"); }
		EnsureAbort(dataLen == sizeof(m_hashSize));
	}


	Hash& Hash::Process(Seq data)
	{
		EnsureThrow(m_haveHash);
	
		if (!CryptHashData(m_hash, (byte*) data.p, NumCast<DWORD>(data.n), 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptHashData"); }
	
		return *this;
	}


	Hash& Hash::Final(byte* pHash)
	{
		EnsureThrow(m_haveHash && m_hashSize);

		DWORD dataLen = m_hashSize;
		if (!CryptGetHashParam(m_hash, HP_HASHVAL, pHash, &dataLen, 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGetHashParam(HP_HASHVAL)"); }
		EnsureAbort(dataLen == m_hashSize);
	
		return *this;
	}


	Hash& Hash::Final(Enc& hash)
	{
		Enc::Write write = hash.IncWrite(m_hashSize);
		Final(write.Ptr());
		write.Add(m_hashSize);
		return *this;
	}


	Str Hash::HashOf(Seq data, ALG_ID algId)
	{
		Str hash;
		Hash().Create(algId).Process(data).Final(hash);
		return hash;
	}



	// SymCrypt

	SymCrypt::~SymCrypt()
	{
		try { Reset_Locked(); }
		catch (Exception const& e) { EnsureFailWithDesc(OnFail::Report, e.what(), __FUNCTION__, __LINE__); }
	}


	void SymCrypt::Reset_Locked()
	{
		DWORD err = 0;

		if (m_haveMacKey)
		{
			if (!CryptDestroyKey(m_macKey) && !err)
				err = GetLastError();

			m_haveMacKey = false;
		}

		if (m_haveEncKey)
		{
			if (!CryptDestroyKey(m_encKey) && !err)
				err = GetLastError();

			m_haveEncKey = false;
		}

		if (err)
			throw WinErr<>(err, __FUNCTION__ ": Error in one or more calls to CryptDestroyKey");
	}


	void SymCrypt::Generate()
	{
		Locker locker(m_mx);

		Reset_Locked();
	
		DWORD encKeyFlags = (256 << 16);	// key size
		if (!CryptGenKey(g_cryptProv, CALG_AES_256, encKeyFlags, &m_encKey))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGenKey(CALG_AES_256)"); }
		m_haveEncKey = true;
	
		try
		{		
			DWORD dataLen = sizeof(m_blockLen);
			if (!CryptGetKeyParam(m_encKey, KP_BLOCKLEN, (BYTE*) &m_blockLen, &dataLen, 0))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGetKeyParam(KP_BLOCKLEN)"); }
			EnsureAbort(dataLen == sizeof(m_blockLen));
			EnsureThrow(m_blockLen != 0);
			EnsureThrow((m_blockLen % 64) == 0);
			m_blockLen /= 8;
		
			DWORD mode = CRYPT_MODE_CBC;
			if (!CryptSetKeyParam(m_encKey, KP_MODE, (BYTE*) &mode, 0))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptSetKeyParam(KP_MODE)"); }

			DWORD macKeyFlags = (256 << 16);	// key size
			if (!CryptGenKey(g_cryptProv, CALG_AES_256, macKeyFlags, &m_macKey))
				{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptGenKey(CALG_AES_256)"); }
			m_haveMacKey = true;
		}
		catch (...)
		{
			Reset();
			throw;
		}
	}


	void SymCrypt::Encrypt(Seq plaintext, Enc& ciphertext)
	{
		Locker locker(m_mx);

		EnsureThrow(m_haveEncKey);
		EnsureThrow(m_haveMacKey);

		// Create hash
		Hash hh;
		hh.CreateHmac(CALG_SHA_256, m_macKey);

		// Resize buffer
		DWORD maxEncryptSize = NumCast<DWORD>(plaintext.n + m_blockLen);
		Enc::Write write = ciphertext.IncWrite(m_blockLen + maxEncryptSize + hh.HashSize());
		byte* pbCiphertext = write.Ptr();

		// Set IV		
		GenRandom(pbCiphertext, m_blockLen);
		if (!CryptSetKeyParam(m_encKey, KP_IV, (BYTE*) pbCiphertext, 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptSetKeyParam(KP_IV)"); }
	
		// Encrypt
		DWORD encryptSize = NumCast<DWORD>(plaintext.n);
		BYTE* pbEncrypt = pbCiphertext + m_blockLen;
		memcpy(pbEncrypt, plaintext.p, plaintext.n);
		if (!CryptEncrypt(m_encKey, hh.m_hash, true, 0, pbEncrypt, &encryptSize, maxEncryptSize))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptEncrypt"); }
		EnsureThrow(encryptSize >= plaintext.n);
		EnsureAbort(encryptSize <= maxEncryptSize);
	
		// Add hash
		hh.Final(pbCiphertext + m_blockLen + encryptSize);
		write.Add(m_blockLen + encryptSize + hh.HashSize());
	}


	bool SymCrypt::Decrypt(Seq ciphertext, Enc& plaintext)
	{
		Locker locker(m_mx);
	
		EnsureThrow(m_haveEncKey);
		EnsureThrow(m_haveMacKey);

		// Create hash
		Hash hh;
		hh.CreateHmac(CALG_SHA_256, m_macKey);

		// Check input
		if (ciphertext.n < (2*m_blockLen) + hh.HashSize())
			return false;			
	
		// Set IV		
		if (!CryptSetKeyParam(m_encKey, KP_IV, ciphertext.p, 0))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptSetKeyParam(KP_IV)"); }
	
		// Decrypt
		DWORD      writeSize      { NumCast<DWORD>(ciphertext.n) - m_blockLen };
		DWORD      decryptSize    { writeSize - hh.HashSize() };
		Enc::Write write          { plaintext.IncWrite(writeSize) };
		byte*      pbPlaintext    { write.Ptr() };
		byte*      pbComputedHash { pbPlaintext + decryptSize };

		memcpy(pbPlaintext, ciphertext.p + m_blockLen, decryptSize);
		if (!CryptDecrypt(m_encKey, hh.m_hash, true, 0, pbPlaintext, &decryptSize))
			{ LastWinErr e; throw e.Make<>(__FUNCTION__ ": CryptDecrypt"); }
		EnsureAbort(decryptSize <= writeSize - hh.HashSize());
	
		// Check hash
		hh.Final(pbComputedHash);
		byte const* pbInputHash = (ciphertext.p + ciphertext.n) - hh.HashSize();
		if (memcmp(pbComputedHash, pbInputHash, hh.HashSize()) != 0)
			return false;

		write.Add(decryptSize);
		return true;
	}



	namespace Cng
	{
		void KeyHandle::DestroyKey(CanThrow canThrow)
		{
			if (m_kh != nullptr)
			{
				NTSTATUS st = Call_BCryptDestroyKey(m_kh);
				if (st != STATUS_SUCCESS)
				{
					if (canThrow == CanThrow::No || std::uncaught_exception())
						EnsureReportWithNr(!"BCryptDestroyKey", st);
					else 
						throw NtStatusErr<>(st, __FUNCTION__ ": BCryptDestroyKey");
				}

				m_kh = nullptr;
			}
		}

		DWORD GetDwordProperty(BCRYPT_HANDLE obj, LPCWSTR propId)
		{
			DWORD value    {};
			ULONG cbResult {};
			NTSTATUS st = Call_BCryptGetProperty(obj, propId, (PUCHAR) &value, sizeof(value), &cbResult, 0);
			if (st != STATUS_SUCCESS)      throw NtStatusErr<>(st, __FUNCTION__ ": BCryptGetProperty");
			if (cbResult != sizeof(value)) throw ZLitErr(__FUNCTION__ ": BCryptGetProperty returned an unexpected number of bytes");
			return value;
		}


		void ExportKey(BCRYPT_KEY_HANDLE kh, LPCWSTR blobType, Enc& blob)
		{
			EnsureThrow(!!kh);

			ULONG exportSize;
			NTSTATUS st = Call_BCryptExportKey(kh, nullptr, blobType, nullptr, 0, &exportSize, 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptExportKey (first)");

			Enc::Write write = blob.IncWrite(exportSize);
			ULONG sizeWritten;
			st = Call_BCryptExportKey(kh, nullptr, blobType, write.Ptr(), exportSize, &sizeWritten, 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptExportKey (second)");

			write.Add(sizeWritten);
		}


		void ImportKey(BCRYPT_ALG_HANDLE ph, LPCWSTR blobType, Seq blob, BCRYPT_KEY_HANDLE& kh)
		{
			NTSTATUS st = Call_BCryptImportKeyPair(ph, nullptr, blobType, &kh, (PUCHAR) blob.p, SatCast<ULONG>(blob.n), 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptImportKeyPair");
		}

	}	// Cng



	// RsaSigner

	RsaSigner& RsaSigner::Generate(ULONG nrBits)
	{
		DestroyKey();

		NTSTATUS st = Call_BCryptGenerateKeyPair(g_phRsa, &m_kh, nrBits, 0);
		if (st != STATUS_SUCCESS)
			throw NtStatusErr<>(st, __FUNCTION__ ": BCryptGenerateKeyPair");

		OnExit destroyKey = [&] { DestroyKey(); };

		st = Call_BCryptFinalizeKeyPair(m_kh, 0);
		if (st != STATUS_SUCCESS)
			throw NtStatusErr<>(st, __FUNCTION__ ": BCryptFinalizeKeyPair");

		destroyKey.Dismiss();
		return *this;
	}


	RsaSigner& RsaSigner::ExportPubPkcs1(Enc& derEncoded)
	{
		Str blobBuf;
		ExportPub(blobBuf);

		EnsureThrow(blobBuf.Len() >= sizeof(BCRYPT_RSAKEY_BLOB));
		BCRYPT_RSAKEY_BLOB const* blob = (BCRYPT_RSAKEY_BLOB const*) blobBuf.Ptr();
		EnsureThrow(blob->Magic == BCRYPT_RSAPUBLIC_MAGIC);
		EnsureThrow(blobBuf.Len() >= sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus);
		byte const* pRead = (byte const*) (blob + 1);
		Seq pubExp { pRead, blob->cbPublicExp }; pRead += pubExp.n;
		Seq modulus { pRead, blob->cbModulus }; pRead += modulus.n;
		EnsureAbort(pRead == ((byte const*) blob) + sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus);

		sizet seqInnerLen = Asn1::EncodeDerUInteger_Size(modulus) + Asn1::EncodeDerUInteger_Size(pubExp);
		sizet seqOuterLen = Asn1::EncodeDerSeqHeader_Size(seqInnerLen) + seqInnerLen;

		Enc::Meter meter = derEncoded.IncMeter(seqOuterLen);
		Asn1::EncodeDerSeqHeader (derEncoded, seqInnerLen);
		Asn1::EncodeDerUInteger  (derEncoded, modulus);
		Asn1::EncodeDerUInteger  (derEncoded, pubExp);
		EnsureThrow(meter.Met());
		return *this;
	}


	RsaSigner& RsaSigner::ExportPubPkcs8(Enc& derEncoded)
	{
		Str pkcs1;
		ExportPubPkcs1(pkcs1);

		Seq oid = Oid::Pkcs1_Rsa;
		sizet algIdContentLen = Asn1::EncodeDerOidHeader_Size(oid.n) + oid.n + Asn1::EncodeDerNull_Size;
		sizet algIdLen        = Asn1::EncodeDerSeqHeader_Size(algIdContentLen) + algIdContentLen;
		sizet bitStrLen       = Asn1::EncodeDerBitStrHeader_Size(pkcs1.Len()) + pkcs1.Len();
		sizet pkcs8ContentLen = algIdLen + bitStrLen;
		sizet pkcs8Len        = Asn1::EncodeDerSeqHeader_Size(pkcs8ContentLen) + pkcs8ContentLen;

		Enc::Meter meter = derEncoded.IncMeter(pkcs8Len);
		Asn1::EncodeDerSeqHeader    (derEncoded, pkcs8ContentLen);
		Asn1::EncodeDerSeqHeader    (derEncoded, algIdContentLen);
		Asn1::EncodeDerOidHeader    (derEncoded, oid.n);
		      EncodeVerbatim        (derEncoded, oid);
		Asn1::EncodeDerNull         (derEncoded);
		Asn1::EncodeDerBitStrHeader (derEncoded, pkcs1.Len());
		      EncodeVerbatim        (derEncoded, pkcs1);
		EnsureThrow(meter.Met());
		return *this;
	}


	RsaSigner& RsaSigner::ImportPriv(Seq blob)
	{
		DestroyKey();
		Cng::ImportKey(g_phRsa, BCRYPT_RSAPRIVATE_BLOB, blob, m_kh);
		return *this;
	}


	RsaSigner& RsaSigner::SignPkcs1(Seq data, ALG_ID cryptHashAlgId, LPCWSTR cngHashAlgId, Enc& sig)
	{
		EnsureThrow(!!m_kh);

		Str digest = Hash::HashOf(data, cryptHashAlgId);

		DWORD      sigLen    = Cng::GetDwordProperty(m_kh, BCRYPT_SIGNATURE_LENGTH);
		ULONG      nWriteMax = sigLen + 1;
		Enc::Write write     = sig.IncWrite(nWriteMax); 
		byte*      pbWrite   = write.Ptr();

		BCRYPT_PKCS1_PADDING_INFO padInfo { cngHashAlgId };
		ULONG bytesWritten {};
		NTSTATUS st = Call_BCryptSignHash(m_kh, &padInfo, digest.Ptr(), (ULONG) digest.Len(), pbWrite, nWriteMax, &bytesWritten, BCRYPT_PAD_PKCS1);
		if (st != STATUS_SUCCESS)
			throw NtStatusErr<>(st, __FUNCTION__ ": BCryptSignHash");
		EnsureAbort(bytesWritten <= nWriteMax);

		write.Add(bytesWritten);
		return *this;
	}



	// RsaFormat

	char const* RsaFormat::GetName(uint n)
	{
		switch (n)
		{
		case Any:   return "Any";
		case Pkcs1: return "Pkcs1";
		case Pkcs8: return "Pkcs8";
		default:    return "<unrecognized>";
		}
	}



	// RsaVerifier

	RsaVerifier& RsaVerifier::ImportPub(Seq blob)
	{
		DestroyKey();
		Cng::ImportKey(g_phRsa, BCRYPT_RSAPUBLIC_BLOB, blob, m_kh);
		return *this;
	}


	bool RsaVerifier::ImportPubPkcs(Seq reader, RsaFormat::E& fmt, Asn1::NonCanon nc)
	{
		DestroyKey();

		// At this point, could be either PKCS#1 or #8
		Seq outerSeq;
		if (!Asn1::DecodeBerSeq(reader, outerSeq, nc, MaxPkcs1PubKeyBytes)) return false;
		if (nc != Asn1::NonCanon::Permit && reader.n != 0)                  return false;

		Seq innerSeq;
		if (outerSeq.FirstByte() == 0x02)
		{
			// PKCS#1 format: SEQUENCE { INTEGER modulus, INTEGER pubExp }

			     if (fmt == RsaFormat::Any   ) fmt = RsaFormat::Pkcs1;
			else if (fmt != RsaFormat::Pkcs1 ) return false;

			innerSeq = outerSeq;
		}
		else
		{
			// PKCS#8 format: SEQUENCE { SEQUENCE algId, BIT STRING { SEQUENCE { INTEGER modulus, INTEGER pubExp } } }

			     if (fmt == RsaFormat::Any   ) fmt = RsaFormat::Pkcs8;
			else if (fmt != RsaFormat::Pkcs8 ) return false;

			Seq algIdSeq, oid, bitStr;
			if (!Asn1::DecodeBerSeq    (outerSeq, algIdSeq, nc, MaxPkcs1PubKeyBytes)) return false;
			if (!Asn1::DecodeBerOid    (algIdSeq, oid,      nc, MaxPkcs1PubKeyBytes)) return false;
			if (!Asn1::DecodeBerBitStr (outerSeq, bitStr,   nc, MaxPkcs1PubKeyBytes)) return false;
			if (!oid.EqualExact(Oid::Pkcs1_Rsa))                                      return false;
			if (nc != Asn1::NonCanon::Permit && outerSeq.n != 0)                      return false;

			if (!Asn1::DecodeBerSeq    (bitStr,   innerSeq, nc, MaxPkcs1PubKeyBytes)) return false;
			if (nc != Asn1::NonCanon::Permit && bitStr.n != 0)                        return false;
		}

		Seq modulus, pubExp;
		if (!Asn1::DecodeBerUInteger(innerSeq, modulus, nc, MaxPkcs1PubKeyBytes)) return false;
		if (!Asn1::DecodeBerUInteger(innerSeq, pubExp,  nc, MaxPkcs1PubKeyBytes)) return false;
		if (nc != Asn1::NonCanon::Permit && innerSeq.n != 0)                      return false;

		Str blob;
		blob.Resize(sizeof(BCRYPT_RSAKEY_BLOB) + pubExp.n + modulus.n);

		BCRYPT_RSAKEY_BLOB* pBlob = (BCRYPT_RSAKEY_BLOB*) blob.Ptr();
		pBlob->Magic       = BCRYPT_RSAPUBLIC_MAGIC;
		pBlob->BitLength   = (ULONG) (modulus.n * 8);
		pBlob->cbPublicExp = (ULONG) pubExp.n;
		pBlob->cbModulus   = (ULONG) modulus.n;
		pBlob->cbPrime1    = 0;
		pBlob->cbPrime2    = 0;

		byte* pWrite = (byte*) (pBlob + 1);
		memcpy(pWrite, pubExp.p,  pubExp.n ); pWrite += pubExp.n;
		memcpy(pWrite, modulus.p, modulus.n); pWrite += modulus.n;
		EnsureAbort(pWrite == blob.Ptr() + blob.Len());

		Cng::ImportKey(g_phRsa, BCRYPT_RSAPUBLIC_BLOB, blob, m_kh);
		return true;
	}


	bool RsaVerifier::VerifyPkcs1(Seq data, ALG_ID cryptHashAlgId, LPCWSTR cngHashAlgId, Seq sig)
	{
		EnsureThrow(!!m_kh);

		Str digest = Hash::HashOf(data, cryptHashAlgId);

		BCRYPT_PKCS1_PADDING_INFO padInfo { cngHashAlgId };
		NTSTATUS st = Call_BCryptVerifySignature(m_kh, &padInfo, digest.Ptr(), (ULONG) digest.Len(), (PUCHAR) sig.p, NumCast<ULONG>(sig.n), BCRYPT_PAD_PKCS1);
		if (st == STATUS_SUCCESS)           return true;
		if (st == STATUS_INVALID_SIGNATURE) return false;
		throw NtStatusErr<>(st, __FUNCTION__ ": BCryptVerifySignature");
	}

}
