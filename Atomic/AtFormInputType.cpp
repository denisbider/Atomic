#include "AtIncludes.h"
#include "AtFormInputType.h"


namespace At
{

	// MaxLenField

	void MaxLenField::EncObj(Enc& s) const
	{
		s.Add("Must be no more than ").UIntDecGrp(mc_nMaxLen).Add(" characters in length.");
	}


	bool MaxLenField::IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const
	{
		bool success = true;

		if (Seq(value).DropUtf8_MaxChars(mc_nMaxLen).n)
			{ success = false; if (errs) errs->Add().SetAdd(friendlyName, " must be no more than ", mc_zMaxLen, " characters in length"); }

		return success;
	}



	// FormInputType

	FormInputType const fit_pw_noMinLen {   "99",   99,  "25" };
	FormInputType const fit_ip4         {   "20",   20,  "20" };
	FormInputType const fit_ip6         {   "50",   50,  "50" };
	FormInputType const fit_dnsName     {  "255",  255,  "40" };
	FormInputType const fit_email       {  "253",  253,  "40" };
	FormInputType const fit_url         {  "999",  999, "100" };
	FormInputType const fit_number      {   "20",   20,  "20" };
	FormInputType const fit_postalCode  {   "20",   20,  "20" };
	FormInputType const fit_salutation  {   "20",   20,  "20" };
	FormInputType const fit_name        {  "100",  100,  "40" };
	FormInputType const fit_nameOrEmail {  "255",  255,  "40" };
	FormInputType const fit_street      {  "100",  100,  "80" };
	FormInputType const fit_desc        {  "999",  999, "100" };
	FormInputType const fit_keywords    {  "999",  999, "100" };
	FormInputType const fit_origin      {  "999",  999, "100" };
	FormInputType const fit_nameList    { "9999", 9999, "100" };
	FormInputType const fit_localPath   {  "255",  255, "100" };



	// FormInputTypeWithMin

	void FormInputTypeWithMin::EncObj(Enc& s) const
	{
		s.Add("Must be at least ").UIntDecGrp(mc_nMinLen).Add(" and no more than ").UIntDecGrp(mc_nMaxLen).Add(" characters in length.");
	}


	bool FormInputTypeWithMin::IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const
	{
		bool success = true;

		Seq reader = value;
		if (mc_nMinLen && reader.DropUtf8_MaxChars(mc_nMinLen-1).ReadUtf8Char() == UINT_MAX)
			{ success = false; if (errs) errs->Add().SetAdd(friendlyName, " must be at least ", mc_zMinLen, " characters in length."); }
		else if (mc_nMaxLen < mc_nMinLen || reader.DropUtf8_MaxChars(mc_nMaxLen - mc_nMinLen).n)
			{ success = false; if (errs) errs->Add().SetAdd(friendlyName, " must be no more than ", mc_zMaxLen, " characters in length."); }

		return success;
	}



	// FormInputTypeWithChars

	void FormInputTypeWithChars::EncObj(Enc& s) const
	{
		__super::EncObj(s);
		s.Add(" May contain ").Add(mc_charDesc).Add(".");
	}


	bool FormInputTypeWithChars::IsValidEx(Seq value, Vec<Str>* errs, Seq friendlyName) const
	{
		bool success = __super::IsValidEx(value, errs, friendlyName);

		if (value.ContainsAnyUtf8CharNotOfType(mc_charCrit, mc_nMaxLen))
			{ success = false; if (errs) errs->Add().SetAdd(friendlyName, " may contain only ", mc_charDesc, "."); }

		return success;
	}



	// TextAreaDims

	TextAreaDims const tad_policy      {   "10000",   10000,  "5", "100" };
	TextAreaDims const tad_centerpiece { "1000000", 1000000, "30", "120" };

}
