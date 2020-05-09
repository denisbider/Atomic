#pragma once

#include "AtRp.h"
#include "AtStr.h"


namespace At
{
	namespace BCrypt
	{

		class Provider : public RefCountable
		{
		public:
			~Provider() { Close(); }

			Provider& Open(LPCWSTR algId, LPCWSTR implementation, DWORD flags);
			Provider& OpenRng       () { return Open(BCRYPT_RNG_ALGORITHM,        nullptr, 0); }
			Provider& OpenEcdhP256  () { return Open(BCRYPT_ECDH_P256_ALGORITHM,  nullptr, 0); }
			Provider& OpenEcdsaP256 () { return Open(BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0); }
			Provider& OpenMd5       () { return Open(BCRYPT_MD5_ALGORITHM,        nullptr, 0); }
			Provider& OpenSha256    () { return Open(BCRYPT_SHA256_ALGORITHM,     nullptr, 0); }
			Provider& OpenSha384    () { return Open(BCRYPT_SHA384_ALGORITHM,     nullptr, 0); }
			Provider& OpenSha512    () { return Open(BCRYPT_SHA512_ALGORITHM,     nullptr, 0); }
			Provider& Close();

			BCRYPT_ALG_HANDLE Handle() const { return m_hProvider; }

			// Available for providers that have been opened with a suitable BCrypt algorithm ID, such as using OpenRng().
			Provider const& GenRandom(PUCHAR p, ULONG n, ULONG flags) const;
			Provider const& GenRandom(void* p, sizet n) const { return GenRandom((PUCHAR) p, NumCast<ULONG>(n), 0); }
			Provider const& SetBufRandom(Str& buf, sizet n) const { buf.Resize(n); return GenRandom(buf.Ptr(), n); }
			
			// Generates a number X such that 0 <= X <= maxVal, with a uniform probability distribution.
			uint64 GenRandomNr(uint64 maxVal) const;
			uint32 GenRandomNr32(uint32 maxVal) const { return (uint32) GenRandomNr(maxVal); }

		private:
			BCRYPT_ALG_HANDLE m_hProvider {};
		};



		class Rng : public Provider
		{
		public:
			Rng() { OpenRng(); }
		};



		template <class T>
		class Blob
		{
		public:
			Str& GetBuf() { return m_buf; }

			T const& Head() const
			{
				EnsureThrow(m_buf.Len() >= sizeof(T));
				return *((T const*) m_buf.Ptr());
			}

			byte*       Tail()       { return (byte*)       (&Head() + 1); }
			byte const* Tail() const { return (byte const*) (&Head() + 1); }

			T& InitHead(ULONG magic)
			{
				m_buf.Resize(sizeof(T));
				((T*) m_buf.Ptr())->cbMagic = magic;
				return *((T*) m_buf.Ptr());
			}

			ULONG FullLen() const { return sizeof(T) + TailLen(Head()); }
			void ResizeToFit() { m_buf.Resize(FullLen()); }

		protected:
			Str m_buf;

			void EnsureTail() const { EnsureThrow(m_buf.Len() >= FullLen()); }

			virtual ULONG TailLen(T const& head) const = 0;
		};



		class EccPublicBlob : public Blob<BCRYPT_ECCKEY_BLOB>
		{
		public:
			byte* Ptr_X()       { EnsureTail(); return     Tail();                }
			Seq   Get_X() const { EnsureTail(); return Seq(Tail(), Head().cbKey); }

			byte* Ptr_Y()       { EnsureTail(); ULONG n = Head().cbKey; return     Tail() + n;     }
			Seq   Get_Y() const { EnsureTail(); ULONG n = Head().cbKey; return Seq(Tail() + n, n); }

		private:
			ULONG TailLen(BCRYPT_ECCKEY_BLOB const& head) const override { return 2 * head.cbKey; }
		};



		class EccPrivateBlob : public EccPublicBlob
		{
		public:
			byte* Ptr_d()       { EnsureTail(); ULONG n = Head().cbKey; return     Tail() + (2*n);     }
			Seq   Get_d() const { EnsureTail(); ULONG n = Head().cbKey; return Seq(Tail() + (2*n), n); }

		private:
			ULONG TailLen(BCRYPT_ECCKEY_BLOB const& head) const override { return 3 * head.cbKey; }
		};



		class Key
		{
		public:
			~Key() noexcept { Destroy(CanThrow::No); }

			Key& GenerateKeyPair(Provider const& provider, ULONG length, ULONG flags);
			Key& FinalizeKeyPair();
			Key& Export(Str& buf, LPCWSTR blobType);
			Key& Import(Provider const& provider, Seq blob, LPCWSTR blobType);
			Key& Sign(void* padInfo, Seq hash, Str& sig, ULONG flags);
			bool Verify(void* padInfo, Seq hash, Seq sig, ULONG flags);
			Key& Destroy(CanThrow canThrow = CanThrow::Yes);

			BCRYPT_KEY_HANDLE Handle() { return m_hKey; }

		private:
			BCRYPT_KEY_HANDLE m_hKey {};
		};



		class Hash
		{
		public:
			~Hash() { Destroy(); }

			Hash& Init(Provider const& provider, Seq secretKey);
			Hash& Init(Provider const& provider) { return Init(provider, Seq()); }
			
			Hash& Begin(Provider const& provider);
			Hash& Update(Seq data);
			Hash& Final(Enc& digest);
			Hash& Destroy();

		private:
			BCRYPT_HASH_HANDLE m_hHash      {};
			DWORD              m_hashLen    {};
			Str                m_obj;
			Str                m_secretKey;
		};

	}
}

