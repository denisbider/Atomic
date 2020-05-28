#include "AtIncludes.h"
#include "AtFormInputType.h"


namespace At
{

	// FormInputType

	void FormInputType::EncObj(Enc& s) const
	{
		s.Add("Must be no more than ").UInt(mc_nMaxLen).Add(" characters in length.");
	}


	bool FormInputType::IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const
	{
		bool success = true;

		if (Seq(value).DropUtf8_MaxChars(mc_nMaxLen).n)
			{ success = false; errs.Add().SetAdd(friendlyName, " must be no more than ", mc_zMaxLen, " characters in length."); }

		return success;
	}


	FormInputType const fit_pw_noMinLen (  "25",   "99",   99 );
	FormInputType const fit_ip4         (  "20",   "20",   20 );
	FormInputType const fit_ip6         (  "50",   "50",   50 );
	FormInputType const fit_dnsName     (  "40",  "255",  255 );
	FormInputType const fit_email       (  "40",  "255",  255 );
	FormInputType const fit_url         ( "100",  "999",  999 );
	FormInputType const fit_number      (  "20",   "20",   20 );
	FormInputType const fit_postalCode  (  "20",   "20",   20 );
	FormInputType const fit_salutation  (  "20",   "20",   20 );
	FormInputType const fit_name        (  "40",  "100",  100 );
	FormInputType const fit_nameOrEmail (  "40",  "255",  255 );
	FormInputType const fit_street      (  "80",  "100",  100 );
	FormInputType const fit_desc        ( "100",  "999",  999 );
	FormInputType const fit_keywords    ( "100",  "999",  999 );
	FormInputType const fit_origin      ( "100",  "999",  999 );
	FormInputType const fit_nameList    ( "100", "9999", 9999 );
	FormInputType const fit_localPath   ( "100",  "255",  255 );



	// FormInputTypeWithMin

	void FormInputTypeWithMin::EncObj(Enc& s) const
	{
		s.Add("Must be at least ").UInt(mc_nMinLen).Add(" and no more than ").UInt(mc_nMaxLen).Add(" characters in length.");
	}


	bool FormInputTypeWithMin::IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const
	{
		bool success = true;

		Seq reader = value;
		if (mc_nMinLen && reader.DropUtf8_MaxChars(mc_nMinLen-1).ReadUtf8Char() == UINT_MAX)
			{ success = false; errs.Add().SetAdd(friendlyName, " must be at least ", mc_zMinLen, " characters in length."); }
		else if (mc_nMaxLen < mc_nMinLen || reader.DropUtf8_MaxChars(mc_nMaxLen - mc_nMinLen).n)
			{ success = false; errs.Add().SetAdd(friendlyName, " must be no more than ", mc_zMaxLen, " characters in length."); }

		return success;
	}



	// FormInputTypeWithChars

	void FormInputTypeWithChars::EncObj(Enc& s) const
	{
		__super::EncObj(s);
		s.Add(" May contain ").Add(mc_charDesc).Add(".");
	}


	bool FormInputTypeWithChars::IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const
	{
		bool success = __super::IsValid(value, errs, friendlyName);

		if (value.ContainsAnyUtf8CharNotOfType(mc_charCrit, mc_nMaxLen))
			{ success = false; errs.Add().SetAdd(friendlyName, " may contain only ", mc_charDesc, "."); }

		return success;
	}

}