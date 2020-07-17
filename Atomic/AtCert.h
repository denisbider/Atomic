#pragma once

#include "AtStr.h"
#include "AtTime.h"


namespace At
{

	// Cert

	class Cert
	{
	public:
		static Str GetNameString(PCCERT_CONTEXT cert, DWORD type, DWORD flags, void* typePara);

		static Str GetSubject_DnsName(PCCERT_CONTEXT cert)
			{ return GetNameString(cert, CERT_NAME_DNS_TYPE, 0, nullptr); }

		static Str GetSubject_SimpleDisplay(PCCERT_CONTEXT cert)
			{ return GetNameString(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_STR_ENABLE_PUNYCODE_FLAG, nullptr); }

		static Str GetIssuer_SimpleDisplay(PCCERT_CONTEXT cert)
			{ return GetNameString(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG | CERT_NAME_STR_ENABLE_PUNYCODE_FLAG, nullptr); }

	public:
		Cert() noexcept {}
		Cert(PCCERT_CONTEXT x) noexcept : m_cert(x) {}		// Takes ownership of the PCCERT_CONTEXT
		Cert(Cert const& x) noexcept { if (x.m_cert) m_cert = CertDuplicateCertificateContext(x.m_cert); }
		Cert(Cert&& x) noexcept { m_cert = x.m_cert; x.m_cert = nullptr; }

		~Cert() noexcept { Clear(); }

		Cert& operator= (PCCERT_CONTEXT x) noexcept { Clear(); m_cert = x; return *this; }		// Takes ownership of the PCCERT_CONTEXT
		Cert& operator= (Cert const& x) noexcept { Clear(); if (x.m_cert) m_cert = CertDuplicateCertificateContext(x.m_cert); return *this; }
		Cert& operator= (Cert&& x) noexcept { Clear(); m_cert = x.m_cert; x.m_cert = nullptr; return *this; }

		void Clear() noexcept { if (m_cert) { CertFreeCertificateContext(m_cert); m_cert = nullptr; } }
		bool Any() const noexcept { return nullptr != m_cert; }

		PCCERT_CONTEXT Ptr() const noexcept { return m_cert; }
		PCCERT_CONTEXT operator-> () const { EnsureThrow(nullptr != m_cert); return m_cert; }
		PCCERT_CONTEXT Dismiss() noexcept { PCCERT_CONTEXT x = m_cert; m_cert = nullptr; return x; }
	
	public:
		Str GetNameString(DWORD type, DWORD flags, void* typePara) const { return GetNameString(m_cert, type, flags, typePara); }
		Str GetSubject_DnsName       () const { return GetSubject_DnsName       (m_cert); }
		Str GetSubject_SimpleDisplay () const { return GetSubject_SimpleDisplay (m_cert); }
		Str GetIssuer_SimpleDisplay  () const { return GetIssuer_SimpleDisplay  (m_cert); }

	private:
		PCCERT_CONTEXT m_cert {};
	};



	// CertInfo

	struct CertInfo
	{
		Str  m_subject;
		Str  m_issuer;
		Time m_notBefore;
		Time m_notAfter;

		void FromCert(Cert const& cert);
		void EncObj(Enc& s) const;
	};



	// CertFitScore

	struct CertFitScore
	{
		bool m_matchesDnsName {};		// false is worse
		Time m_timeFromInvalidity;		// less is worse

		static CertFitScore Assess(Cert const& cert, Seq dnsName, Time now);

		// Returns 1 if this CertFitScore is better than parameter, 0 if equal, -1 if parameter score is better
		int Compare(CertFitScore const& x) const noexcept;

		bool operator>  (CertFitScore const& x) const noexcept { return Compare(x) >  0; }
		bool operator<  (CertFitScore const& x) const noexcept { return Compare(x) <  0; }
		bool operator>= (CertFitScore const& x) const noexcept { return Compare(x) >= 0; }
		bool operator<= (CertFitScore const& x) const noexcept { return Compare(x) <= 0; }
		bool operator== (CertFitScore const& x) const noexcept { return Compare(x) == 0; }
		bool operator!= (CertFitScore const& x) const noexcept { return Compare(x) != 0; }
	};

}
