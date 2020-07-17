#pragma once

#include "AtIncludes.h"
#include "AtCert.h"
#include "AtSchannel.h"
#include "AtSlice.h"


namespace At
{

	class CertStore
	{
	public:
		CertStore(DWORD storeLocation, void const* storeParam) : mc_storeLocation(storeLocation), mc_storeParam(storeParam) {}
		~CertStore();

		void Open();
		void Close();

		bool IsOpen() const { return !!m_store; }

		// Returns whether the certificate was found. "conn" can be nullptr to only check for certificate existence without adding it.
		// "certInfo" can be nullptr if no info is needed. If not null, it receives certificate info if any certificate is found.
		bool GetCert_ByHash(Schannel* conn, CertInfo* certInfo, Seq certHash);
	
		// Returns whether a certificate was found. "conn" can be nullptr to only check for certificate existence without adding it.
		// "certInfo" can be nullptr if no info is needed. If not null, it receives certificate info if any certificate is found.
		bool GetCert_FirstNonExpired(Schannel* conn, CertInfo* certInfo);

		// Returns whether a certificate was found. "conn" can be nullptr to only check for certificate existence without adding it.
		// "certInfo" can be nullptr if no info is needed. If not null, it receives certificate info if any certificate is found.
		bool GetCert_BestFitScore(Schannel* conn, CertInfo* certInfo, Seq dnsName);
	
		// Returns the number of certificates found. "conn" can be nullptr to only check for certificate existence without adding it.
		// "certInfo" can be nullptr if no info is needed. If not null, it receives certificate info if any certificates are found.
		// "dnsNames" can be empty to get all non-expired certificates. If non-empty, only certificates matching one of the CNs will be found.
		sizet GetCerts_AllNonExpired(Schannel* conn, Vec<CertInfo>* certInfo, Slice<Seq> dnsNames);

		// Returns whether a certificate was found. "conn" can be nullptr to only check for certificate existence without adding it.
		// If certHash is non-empty, attempts to find certificate by hash. If not found, does NOT fall back to first non-expired cert.
		// If certHash is empty, attempts to find first non-expired cert.
		// "certInfo" can be nullptr if no info is needed. If not null, it receives certificate info if any certificate is found.
		bool GetCert_ByHash_FirstNonExpiredIfNoHash(Schannel* conn, Seq certHash, CertInfo* certInfo);

	private:
		DWORD const mc_storeLocation;
		void const* const mc_storeParam;

		HCERTSTORE m_store {};
	};


	class CertStore_LocalMachine_My : public CertStore
	{
	public:
		CertStore_LocalMachine_My() : CertStore(CERT_SYSTEM_STORE_LOCAL_MACHINE, L"MY") {}
	};

}
