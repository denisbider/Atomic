#pragma once

#include "AtStr.h"
#include "AtVec.h"


namespace At
{

	// FormInputType

	struct FormInputType
	{
		char const* const mc_zWidth  {};
		char const* const mc_zMaxLen {};
		uint const        mc_nMaxLen {};

		FormInputType(char const* zWidth, char const* zMaxLen, uint nMaxLen)
			: mc_zWidth(zWidth), mc_zMaxLen(zMaxLen), mc_nMaxLen(nMaxLen) {}

		virtual void EncObj(Enc& s) const;
		virtual bool IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const;
	};


	extern FormInputType const fit_pw_noMinLen;
	extern FormInputType const fit_ip4;
	extern FormInputType const fit_ip6;
	extern FormInputType const fit_dnsName;
	extern FormInputType const fit_email;
	extern FormInputType const fit_url;
	extern FormInputType const fit_number;
	extern FormInputType const fit_postalCode;
	extern FormInputType const fit_salutation;
	extern FormInputType const fit_name;
	extern FormInputType const fit_nameOrEmail;
	extern FormInputType const fit_street;
	extern FormInputType const fit_desc;
	extern FormInputType const fit_keywords;
	extern FormInputType const fit_origin;
	extern FormInputType const fit_nameList;
	extern FormInputType const fit_localPath;



	// FormInputTypeWithMin

	struct FormInputTypeWithMin : FormInputType
	{
		char const* const mc_zMinLen {};
		uint const        mc_nMinLen {};

		FormInputTypeWithMin(char const* zWidth, char const* zMaxLen, uint nMaxLen, char const* zMinLen, uint nMinLen)
			: FormInputType(zWidth, zMaxLen, nMaxLen), mc_zMinLen(zMinLen), mc_nMinLen(nMinLen) {}

		void EncObj(Enc& s) const override;
		bool IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const override;
	};



	// FormInputTypeWithChars

	struct FormInputTypeWithChars : FormInputTypeWithMin
	{
		FormInputTypeWithChars(char const* zWidth, char const* zMaxLen, uint nMaxLen, char const* zMinLen, uint nMinLen, CharCriterion charCrit, char const* charDesc)
			: FormInputTypeWithMin(zWidth, zMaxLen, nMaxLen, zMinLen, nMinLen), mc_charCrit(charCrit), mc_charDesc(charDesc) {}

		CharCriterion const mc_charCrit;
		char const*   const mc_charDesc;

		void EncObj(Enc& s) const override;
		bool IsValid(Seq value, Vec<Str>& errs, Seq friendlyName) const override;
	};

}
