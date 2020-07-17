#include "AtIncludes.h"
#include "AtCert.h"

#include "AtUtfWin.h"


namespace At
{

	// Cert

	Str Cert::GetNameString(PCCERT_CONTEXT cert, DWORD type, DWORD flags, void* typePara)
	{
		DWORD nrChars = CertGetNameStringW(cert, type, flags, typePara, nullptr, 0);
		if (1 >= nrChars)
			return Str();

		Vec<wchar_t> nameBuf;
		nameBuf.ResizeExact(nrChars);
		nrChars = CertGetNameStringW(cert, type, flags, typePara, nameBuf.Ptr(), nrChars);
		nameBuf.ResizeExact(nrChars);

		while (nameBuf.Any() && !nameBuf.Last())
			nameBuf.PopLast();

		Str result;
		FromUtf16(nameBuf.Ptr(), NumCast<int>(nameBuf.Len()), result, CP_UTF8);
		return result;
	}



	// CertInfo

	void CertInfo::FromCert(Cert const& cert)
	{
		EnsureThrow(cert.Any());
		m_subject = cert.GetSubject_SimpleDisplay();
		m_issuer = cert.GetIssuer_SimpleDisplay();
		m_notBefore = Time::FromFt(cert->pCertInfo->NotBefore);
		m_notAfter = Time::FromFt(cert->pCertInfo->NotAfter);
	}


	void CertInfo::EncObj(Enc& s) const
	{
		s.Add("Subject: ");
		if (m_subject.Any()) s.Add(m_subject); else s.Add("(none)");

		s.Add(", Issuer: ");
		if (m_issuer.Any()) s.Add(m_issuer); else s.Add("(none)");

		s.Add(", Valid from: ");
		if (m_notBefore.Any()) s.Obj(m_notBefore, TimeFmt::IsoSecZ); else s.Add("(none)");

		s.Add(", Until: ");
		if (m_notAfter.Any()) s.Obj(m_notAfter, TimeFmt::IsoSecZ); else s.Add("(none)");
	}


	
	// CertFitScore

	CertFitScore CertFitScore::Assess(Cert const& cert, Seq dnsName, Time now)
	{
		CertFitScore x;
		if (dnsName.EqualInsensitive(cert.GetSubject_DnsName()))
			x.m_matchesDnsName = true;

		x.m_timeFromInvalidity = Time::Max();

		Time timeSince = now - Time::FromFt(cert->pCertInfo->NotBefore);
		if (timeSince < Time::FromHours(36))
			x.m_timeFromInvalidity = timeSince;

		Time timeUntil = Time::FromFt(cert->pCertInfo->NotAfter) - now;
		if (x.m_timeFromInvalidity > timeUntil)
			x.m_timeFromInvalidity = timeUntil;

		return x;
	}


	int CertFitScore::Compare(CertFitScore const& x) const noexcept
	{
		if (m_matchesDnsName != x.m_matchesDnsName)
			return (m_matchesDnsName ? 1 : 0) - (x.m_matchesDnsName ? 1 : 0);

		if (m_timeFromInvalidity >  x.m_timeFromInvalidity) return 1;
		if (m_timeFromInvalidity == x.m_timeFromInvalidity) return 0;
		return -1;
	}

}
