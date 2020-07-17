#include "AtIncludes.h"
#include "AtCertStore.h"


namespace At
{

	CertStore::~CertStore()
	{
		try { Close(); }
		catch (Exception const& e) { EnsureFailWithDesc(OnFail::Report, e.what(), __FUNCTION__, __LINE__); }
	}


	void CertStore::Open()
	{
		EnsureThrow(!m_store);

		DWORD flags = mc_storeLocation | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;
		m_store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, NULL, flags, mc_storeParam);
		if (!m_store)
			{ LastWinErr e; throw e.Make<>("Error in CertOpenStore"); }
	}


	void CertStore::Close()
	{
		if (!!m_store)
		{
			if (!CertCloseStore(m_store, 0))
				{ LastWinErr e; throw e.Make<>("Error in CertCloseStore"); }
		}
	}


	bool CertStore::GetCert_ByHash(Schannel* conn, CertInfo* certInfo, Seq certHash)
	{
		EnsureThrow(!!m_store);

		CRYPT_HASH_BLOB chb;
		chb.pbData = (BYTE*) certHash.p;
		chb.cbData = NumCast<DWORD>(certHash.n);
	
		Cert cert { CertFindCertificateInStore(m_store, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, 0, CERT_FIND_HASH, &chb, nullptr) };
		if (!cert.Any())
		{
			LastWinErr e;
			if (CRYPT_E_NOT_FOUND == e.m_err)
				return false;
		
			throw e.Make<>("Error in CertFindCertificateInStore");
		}

		if (conn)
			conn->AddCert(cert);
		if (certInfo)
			certInfo->FromCert(cert);

		return true;
	}


	bool CertStore::GetCert_FirstNonExpired(Schannel* conn, CertInfo* certInfo)
	{
		EnsureThrow(!!m_store);
	
		Cert cert { CertEnumCertificatesInStore(m_store, NULL) };
		while (true)
		{
			if (!cert.Any())
			{
				LastWinErr e;
				if (CRYPT_E_NOT_FOUND   == e.m_err ||
					ERROR_NO_MORE_FILES == e.m_err)
					return false;

				throw e.Make<>("Error in CertEnumCertificatesInStore");
			}

			if (CertVerifyTimeValidity(NULL, cert->pCertInfo) == 0)
			{
				if (conn)
					conn->AddCert(cert);
				if (certInfo)
					certInfo->FromCert(cert);

				return true;
			}

			cert = CertEnumCertificatesInStore(m_store, cert.Dismiss());
		}
	}


	bool CertStore::GetCert_BestFitScore(Schannel* conn, CertInfo* certInfo, Seq dnsName)
	{
		EnsureThrow(!!m_store);
	
		Cert bestFitCert;
		CertFitScore bestFitScore;

		{
			Cert cert { CertEnumCertificatesInStore(m_store, NULL) };
			Time now = Time::NonStrictNow();

			while (true)
			{
				if (!cert.Any())
				{
					LastWinErr e;
					if (CRYPT_E_NOT_FOUND   == e.m_err ||
						ERROR_NO_MORE_FILES == e.m_err)
						break;

					throw e.Make<>("Error in CertEnumCertificatesInStore");
				}

				if (CertVerifyTimeValidity(NULL, cert->pCertInfo) == 0)
				{
					CertFitScore score = CertFitScore::Assess(cert, dnsName, now);
					if (score > bestFitScore)
					{
						bestFitCert = cert;
						bestFitScore = score;
					}
				}

				cert = CertEnumCertificatesInStore(m_store, cert.Dismiss());
			}
		}

		if (bestFitCert.Any())
		{
			if (conn)
				conn->AddCert(bestFitCert);
			if (certInfo)
				certInfo->FromCert(bestFitCert);

			return true;
		}

		return false;
	}


	sizet CertStore::GetCerts_AllNonExpired(Schannel* conn, Vec<CertInfo>* certInfo, Slice<Seq> dnsNames)
	{
		EnsureThrow(!!m_store);
	
		Cert cert { CertEnumCertificatesInStore(m_store, NULL) };
		sizet nrFound {};

		while (true)
		{
			if (!cert.Any())
			{
				LastWinErr e;
				if (CRYPT_E_NOT_FOUND   == e.m_err ||
					ERROR_NO_MORE_FILES == e.m_err)
					return nrFound;

				throw e.Make<>("Error in CertEnumCertificatesInStore");
			}

			if (CertVerifyTimeValidity(NULL, cert->pCertInfo) == 0)
			{
				bool match {};
				if (!dnsNames.Any())
					match = true;
				else
				{
					Str certDnsName = cert.GetSubject_DnsName();
					if (certDnsName.Any())
						for (Seq dnsName : dnsNames)
							if (dnsName.EqualInsensitive(certDnsName))
								{ match = true; break; }
				}
				
				if (match)
				{
					++nrFound;

					if (conn)
						conn->AddCert(cert);
					if (certInfo)
						certInfo->Add().FromCert(cert);
				}
			}

			cert = CertEnumCertificatesInStore(m_store, cert.Dismiss());
		}
	}


	bool CertStore::GetCert_ByHash_FirstNonExpiredIfNoHash(Schannel* conn, Seq certHash, CertInfo* certInfo)
	{
		if (certHash.n)
			return GetCert_ByHash(conn, certInfo, certHash);
		else
			return GetCert_FirstNonExpired(conn, certInfo);
	}

}
