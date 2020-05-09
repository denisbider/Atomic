#pragma once

#include "AtAsn1.h"
#include "AtMutex.h"
#include "AtStr.h"


namespace At
{

	namespace Crypt
	{
		// Init() can be called multiple times, but must be followed by the same number of calls to Free().
		void Init();
		void Free();

		bool Inited();

		class Initializer : public NoCopy
		{
		public:
			Initializer() { Init(); }
			~Initializer() { Free(); }
		};
	
		// GenRandom
		void GenRandom(void* buffer, uint len);

		uint64 GenRandomNr(uint64 maxVal);
		inline uint32 GenRandomNr32(uint32 maxVal) { return (uint) GenRandomNr(maxVal); }

		// Calls CryptProtectData. Pass Windows flags such as CRYPTPROTECT_LOCAL_MACHINE, CRYPTPROTECT_UI_FORBIDDEN, and/or CRYPTPROTECT_AUDIT
		void ProtectData(Enc& enc, Seq in, DWORD flags);

		// Calls CryptUnprotectData. Pass Windows flags such as CRYPTPROTECT_UI_FORBIDDEN and/or CRYPTPROTECT_VERIFY_PROTECTION
		void UnprotectData(Enc& enc, Seq in, DWORD flags);

		inline Str ProtectData   (Seq in, DWORD flags) { Str out; ProtectData   (out, in, flags); return out; }
		inline Str UnprotectData (Seq in, DWORD flags) { Str out; UnprotectData (out, in, flags); return out; }
	};



	// Hash
	// Methods are NOT thread-safe.
	
	struct Hash : NoCopy
	{
	public:
		Hash() : m_haveHash(false) {}
		~Hash();
	
		Hash& Close();
	
		Hash& Create(ALG_ID algId);
		Hash& CreateHmac(ALG_ID algId, HCRYPTKEY macKey);
	
		DWORD HashSize() const { return m_hashSize; }
	
		Hash& Process(Seq data);
		Hash& Final(byte* pHash);
		Hash& Final(Enc& hash);

		static Str HashOf(Seq data, ALG_ID algId);
		static Str HexHashOf(Seq data, ALG_ID algId, CharCase cc = CharCase::Upper) { return Str().Hex(HashOf(data, algId), cc); }

	public:
		bool m_haveHash;
		HCRYPTHASH m_hash;
		DWORD m_hashSize;

		void InitHashSize();
	};



	// SymCrypt
	// Encryptor and decryptor using AES256-CBC, HMAC-SHA2-256, and ephemeral keys.
	// Used by WebRequestHandler to encrypt cookies, e.g. confirmation cookies.
	// Methods are thread-safe: WebRequestHandler usage case is one static object per process.

	class SymCrypt : NoCopy
	{
	public:
		SymCrypt() : m_haveEncKey(false), m_haveMacKey(false) {}
		~SymCrypt();

		void Reset() { Locker locker(m_mx); Reset_Locked(); }
		void Generate();
		void Encrypt(Seq plaintext, Enc& ciphertext);
		bool Decrypt(Seq ciphertext, Enc& plaintext);

	private:
		Mutex m_mx;
		bool m_haveEncKey, m_haveMacKey;
		HCRYPTKEY m_encKey, m_macKey;
		DWORD m_blockLen;
	
		void Reset_Locked();
	};



	// CNG utility classes and functions
	// NOTE: This needs to be migrated to AtBCrypt.h/.cpp.
	
	namespace Cng
	{
		class KeyHandle : NoCopy
		{
		protected:
			KeyHandle() noexcept = default;
			KeyHandle(KeyHandle&&) noexcept = default;

			~KeyHandle() noexcept { DestroyKey(CanThrow::No); }

			void DestroyKey(CanThrow canThrow = CanThrow::Yes);

			BCRYPT_KEY_HANDLE m_kh {};
		};

		DWORD GetDwordProperty(BCRYPT_HANDLE obj, LPCWSTR propId);
		void  ExportKey(BCRYPT_KEY_HANDLE kh, LPCWSTR blobType, Enc& blob);
		void  ImportKey(BCRYPT_ALG_HANDLE ph, LPCWSTR blobType, Seq blob, BCRYPT_KEY_HANDLE& kh);
	}



	// RsaSigner and RsaVerifier.
	// Used to generate and verify DKIM signatures.
	// Methods are NOT thread-safe.

	class RsaSigner : Cng::KeyHandle
	{
	public:
		RsaSigner() noexcept = default;
		RsaSigner(RsaSigner&&) noexcept = default;

		RsaSigner& Clear            () { DestroyKey(); return *this; }
		RsaSigner& Generate         (ULONG nrBits);
		RsaSigner& ExportPriv       (Enc& blob) { Cng::ExportKey(m_kh, BCRYPT_RSAPRIVATE_BLOB, blob); return *this; }	// Exports private key in Windows CNG format
		RsaSigner& ExportPub        (Enc& blob) { Cng::ExportKey(m_kh, BCRYPT_RSAPUBLIC_BLOB,  blob); return *this; }	// Exports public key in Windows CNG format
		RsaSigner& ExportPubPkcs1   (Enc& derEncoded);																	// Exports public key in PKCS#1 DER-encoded format
		RsaSigner& ExportPubPkcs8   (Enc& derEncoded);																	// Exports public key in PKCS#8 DER-encoded format
		RsaSigner& ImportPriv       (Seq blob);																				// Imports private key in Windows CNG format

		RsaSigner& SignPkcs1        (Seq data, ALG_ID cryptHashAlgId, LPCWSTR cngHashAlgId, Enc& sig);
		RsaSigner& SignPkcs1_Sha1   (Seq data, Enc& sig) { return SignPkcs1(data, CALG_SHA1,    BCRYPT_SHA1_ALGORITHM,   sig); }
		RsaSigner& SignPkcs1_Sha256 (Seq data, Enc& sig) { return SignPkcs1(data, CALG_SHA_256, BCRYPT_SHA256_ALGORITHM, sig); }
	};


	struct RsaFormat
	{
		enum E
		{
			Any,
			Pkcs1,
			Pkcs8,
		};

		static char const* GetName(uint n);
	};


	class RsaVerifier : Cng::KeyHandle
	{
	public:
		enum { MaxPkcs1PubKeyBytes = 2000 };

		RsaVerifier() noexcept = default;
		RsaVerifier(RsaVerifier&&) noexcept = default;

		RsaVerifier& Clear              () { DestroyKey(); return *this; }
		RsaVerifier& ImportPub          (Seq blob);								// Imports public key in Windows CNG format

		// Imports public key in PKCS#1 or PKCS#8 BER-encoded format. Specify RsaFormat::Any to allow either format,
		// or a particular format to allow just that one. If Any is passed, fmt is set to the format actually decoded (or attempted).
		bool ImportPubPkcs(Seq reader, RsaFormat::E& fmt, Asn1::NonCanon nc);

		bool VerifyPkcs1        (Seq data, ALG_ID cryptHashAlgId, LPCWSTR cngHashAlgId, Seq sig);
		bool VerifyPkcs1_Sha1   (Seq data, Seq sig) { return VerifyPkcs1(data, CALG_SHA1,    BCRYPT_SHA1_ALGORITHM,   sig); }
		bool VerifyPkcs1_Sha256 (Seq data, Seq sig) { return VerifyPkcs1(data, CALG_SHA_256, BCRYPT_SHA256_ALGORITHM, sig); }
	};


}
