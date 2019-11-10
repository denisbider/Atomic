#include "AtIncludes.h"
#include "AtBCrypt.h"

#include "AtDllBCrypt.h"
#include "AtNumCast.h"
#include "AtWinErr.h"


namespace At
{
	namespace BCrypt
	{

		namespace
		{

			DWORD GetProperty_DWORD(BCRYPT_HANDLE obj, LPCWSTR propId)
			{
				DWORD value    {};
				ULONG cbResult {};
				NTSTATUS st = Call_BCryptGetProperty(obj, propId, (PUCHAR) &value, sizeof(value), &cbResult, 0);
				if (st != STATUS_SUCCESS)      throw NtStatusErr<>(st, __FUNCTION__ ": Error in BCryptGetProperty");
				if (cbResult != sizeof(value)) throw ZLitErr(__FUNCTION__ ": BCryptGetProperty returned an unexpected number of bytes");
				return value;
			}

		} // anon



		// Provider

		Provider& Provider::Open(LPCWSTR algId, LPCWSTR implementation, DWORD flags)
		{
			Close();

			NTSTATUS st = Call_BCryptOpenAlgorithmProvider(&m_hProvider, algId, implementation, flags);
			if (st != STATUS_SUCCESS)
			{
				m_hProvider = nullptr;
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptOpenAlgorithmProvider");
			}

			return *this;
		}


		Provider& Provider::Close()
		{
			if (m_hProvider)
			{
				NTSTATUS st = Call_BCryptCloseAlgorithmProvider(m_hProvider, 0);
				m_hProvider = nullptr;
				if (st != STATUS_SUCCESS && !std::uncaught_exception())
					throw NtStatusErr<>(st, __FUNCTION__ ": BCryptCloseAlgorithmProvider");
			}

			return *this;
		}


		Provider const& Provider::GenRandom(PUCHAR p, ULONG n, ULONG flags) const
		{
			NTSTATUS st = Call_BCryptGenRandom(m_hProvider, p, n, flags);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptGenRandom");

			return *this;
		}


		uint64 Provider::GenRandomNr(uint64 maxVal) const
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



		// Key

		Key& Key::GenerateKeyPair(Provider const& provider, ULONG length, ULONG flags)
		{
			Destroy();

			NTSTATUS st = Call_BCryptGenerateKeyPair(provider.Handle(), &m_hKey, length, flags);
			if (st != STATUS_SUCCESS)
			{
				m_hKey = nullptr;
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptGenerateKeyPair");
			}

			return *this;
		}


		Key& Key::FinalizeKeyPair()
		{
			NTSTATUS st = Call_BCryptFinalizeKeyPair(m_hKey, 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptFinalizeKeyPair");

			return *this;
		}


		Key& Key::Export(Str& buf, LPCWSTR blobType)
		{
			ULONG result {};
			NTSTATUS st = Call_BCryptExportKey(m_hKey, nullptr, blobType, nullptr, 0, &result, 0);
			if (st != STATUS_SUCCESS && st != STATUS_BUFFER_TOO_SMALL)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptExportKey (first)");

			buf.Resize(result);

			st = Call_BCryptExportKey(m_hKey, nullptr, blobType, buf.Ptr(), result, &result, 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptExportKey (second)");
			if (result > buf.Len())
				throw StrErr(__FUNCTION__ ": BCryptExportKey: Unexpected result size");

			buf.Resize(result);
			return *this;
		}


		Key& Key::Import(Provider const& provider, Seq blob, LPCWSTR blobType)
		{
			Destroy();

			NTSTATUS st = Call_BCryptImportKeyPair(provider.Handle(), nullptr, blobType, &m_hKey, (PUCHAR) blob.p, NumCast<ULONG>(blob.n), 0);
			if (st != STATUS_SUCCESS)
			{
				m_hKey = nullptr;
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptImportKeyPair");
			}

			return *this;
		}


		Key& Key::Sign(void* padInfo, Seq hash, Str& sig, ULONG flags)
		{
			DWORD sigLen = GetProperty_DWORD(m_hKey, BCRYPT_SIGNATURE_LENGTH);
			sig.Resize(sigLen);

			ULONG result {};
			NTSTATUS st = Call_BCryptSignHash(m_hKey, padInfo, (PUCHAR) hash.p, NumCast<ULONG>(hash.n), (PUCHAR) sig.Ptr(), sigLen, &result, flags);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptSignHash");
			if (result != sig.Len())
				throw StrErr(__FUNCTION__ ": BCryptSignHash: Unexpected result size");
		
			return *this;
		}


		bool Key::Verify(void* padInfo, Seq hash, Seq sig, ULONG flags)
		{
			NTSTATUS st = Call_BCryptVerifySignature(m_hKey, padInfo, (PUCHAR) hash.p, NumCast<ULONG>(hash.n), (PUCHAR) sig.p, NumCast<ULONG>(sig.n), flags);
			if (st == STATUS_SUCCESS)           return true;
			if (st == STATUS_INVALID_SIGNATURE) return false;
			throw NtStatusErr<>(st, __FUNCTION__ ": BCryptVerifySignature");
		}
		

		Key& Key::Destroy()
		{
			if (m_hKey)
			{
				NTSTATUS st = Call_BCryptDestroyKey(m_hKey);
				m_hKey = nullptr;
				if (st != STATUS_SUCCESS && !std::uncaught_exception())
					throw NtStatusErr<>(st, __FUNCTION__ ": BCryptDestroyKey");
			}

			return *this;
		}



		// Hash

		Hash& Hash::Init(Provider const& provider, Seq secretKey)
		{
			DWORD objLen = GetProperty_DWORD(provider.Handle(), BCRYPT_OBJECT_LENGTH);
			m_hashLen = GetProperty_DWORD(provider.Handle(), BCRYPT_HASH_LENGTH);
			m_obj.Resize(objLen);
			m_secretKey = secretKey;
			return *this;
		}


		Hash& Hash::Begin(Provider const& provider)
		{
			Destroy();

			PUCHAR secretKeyPtr {};
			if (m_secretKey.Any())
				secretKeyPtr = (PUCHAR) m_secretKey.Ptr();

			NTSTATUS st = Call_BCryptCreateHash(provider.Handle(), &m_hHash, (PUCHAR) m_obj.Ptr(), (ULONG) m_obj.Len(), secretKeyPtr, (ULONG) m_secretKey.Len(), 0);
			if (st != STATUS_SUCCESS)
			{
				m_hHash = nullptr;
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptCreateHash");
			}

			return *this;
		}


		Hash& Hash::Update(Seq data)
		{
			NTSTATUS st = Call_BCryptHashData(m_hHash, (PUCHAR) data.p, NumCast<ULONG>(data.n), 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptHashData");

			return *this;
		}


		Hash& Hash::Final(Enc& digest)
		{
			Enc::Write write = digest.IncWrite(m_hashLen);
			NTSTATUS st = Call_BCryptFinishHash(m_hHash, write.Ptr(), m_hashLen, 0);
			if (st != STATUS_SUCCESS)
				throw NtStatusErr<>(st, __FUNCTION__ ": BCryptFinishHash");

			write.Add(m_hashLen);
			return *this;
		}


		Hash& Hash::Destroy()
		{
			if (m_hHash)
			{
				NTSTATUS st = Call_BCryptDestroyHash(m_hHash);
				m_hHash = nullptr;
				if (st != STATUS_SUCCESS && !std::uncaught_exception())
					throw NtStatusErr<>(st, __FUNCTION__ ": BCryptDestroyHash");
			}

			return *this;
		}
		
	}
}
